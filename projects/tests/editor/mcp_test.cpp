// The editor's MCP server: the protocol's messages, the tools on a project
// in memory, and a client on this machine asking over HTTP.
#include "bubble/audio/audio.hpp"
#include "bubble/core/log.hpp"
#include "bubble/editor/editor_mcp.hpp"
#include "bubble/editor/editor_project.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/world/transform.hpp"
#include <doctest.h>

// A client's socket: Windows' headers last, as they define words as macros.
#if defined( _WIN32 )
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using namespace bubble;

namespace
{
Json Parse( string_view text )
{
    auto jsonMaybe = Json::TryParse( text );
    REQUIRE_MESSAGE( jsonMaybe, jsonMaybe.error() );
    return *jsonMaybe;
}

// A tool's text, from a call's result.
string TextOf( const Json& result )
{
    const auto contentMaybe = result.TryFind( "content" );
    REQUIRE( contentMaybe );
    const Json::Array& content = *contentMaybe->TryAs<Json::Array>();
    REQUIRE( not content.empty() );
    return *content.front().TryFind( "text" )->TryAs<string>();
}

bool Failed( const Json& result )
{
    return *result.TryFind( "isError" )->TryAs<bool>();
}

McpTool Echo()
{
    return McpTool{ .mName = "echo",
                    .mDescription = "Says what it is told",
                    .mInputSchema = Parse( R"({ "type": "object", "properties": { "text": { "type": "string" } } })" ),
                    .mTryCall = []( const Json& args ) -> expected<Json::Array, string> {
                        const auto textMaybe = args.TryFind( "text" );
                        if ( not textMaybe )
                            return unexpected( "text: what to say expected"s );
                        return Json::Array{ McpText( *textMaybe->TryAs<string>() ) };
                    } };
}

constexpr string_view cProject = R"({
  "version": 1,
  "name": "test",
  "start_level": { "path": "levels/main.scene" },
  "actions": {}
})";

constexpr string_view cLevel = R"({
  "version": 1,
  "root": {
    "children": [
      { "name": "lamp", "components": {
          "transform": { "position": [1, 2, 3] },
          "point_light": { "color": [1, 1, 1, 1], "intensity": 5, "range": 10 } },
        "children": [ { "name": "bulb", "components": { "transform": {} } } ] },
      { "name": "floor", "components": { "transform": {} } }
    ]
  }
})";

struct Project
{
    hmap<string, string> mFiles{ { "project.bubble", string( cProject ) }, { "levels/main.scene", string( cLevel ) } };
    Scope<AudioDevice> mAudio = AudioDevice::Offline();
    Scope<EditorProject> mProject;
    vector<McpTool> mTools;

    Project()
    {
        const ProjectFiles files = ProjectFiles::InMemory( mFiles );
        auto projectMaybe =
            EditorProject::TryOpen( files, AssetRegistry::AtOnce( [files]( const AssetId& assetId, string_view kind ) {
                                        return files.TryRead( ImportedPath( assetId, kind ) );
                                    } ),
                                    *mAudio );
        REQUIRE_MESSAGE( projectMaybe, projectMaybe.error() );
        mProject = std::move( *projectMaybe );
        REQUIRE( mProject->TryOpenStartLevel() );
        mTools = EditorMcpTools( *mProject );
    }

    // A tool called as the server calls it: the text it answers, or its error.
    expected<string, string> Call( string_view name, string_view args = "{}" )
    {
        const auto tool = ranges::find( mTools, name, &McpTool::mName );
        REQUIRE( tool != mTools.end() );
        auto contentMaybe = tool->mTryCall( Parse( args ) );
        if ( not contentMaybe )
            return unexpected( contentMaybe.error() );
        return *contentMaybe->front().TryFind( "text" )->TryAs<string>();
    }

    const Scene& GetScene() { return mProject->GetWorkspace().TryActive()->GetScene(); }
    vec3 LampAt()
    {
        return GetScene()
            .Get<Transform>( *TryResolve( GetScene(), EntityPath{ *Text::TryFrom( "/lamp" ) } ) )
            .mPosition;
    }
};

// ---- a client over HTTP ---------------------------------------------------------

#if defined( _WIN32 )
using SocketHandle = SOCKET;
void CloseSocket( SocketHandle socket )
{
    closesocket( socket );
}
#else
using SocketHandle = int;
void CloseSocket( SocketHandle socket )
{
    close( socket );
}
#endif

// One request to the server on this machine, its reply whole: the status
// line and headers, then the body.
string Request( u16 port, string_view request )
{
#if defined( _WIN32 )
    WSADATA data{};
    REQUIRE( WSAStartup( MAKEWORD( 2, 2 ), &data ) == 0 );
#endif
    const SocketHandle client = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons( port );
    address.sin_addr.s_addr = htonl( INADDR_LOOPBACK );
    REQUIRE( connect( client, reinterpret_cast<const sockaddr*>( &address ), sizeof( address ) ) == 0 );
    REQUIRE( send( client, request.data(), static_cast<int>( request.size() ), 0 ) ==
             static_cast<int>( request.size() ) );
    string reply;
    array<char, 4096> buffer{};
    for ( ;; )
    {
        const auto received = recv( client, buffer.data(), static_cast<int>( buffer.size() ), 0 );
        if ( received <= 0 )
            break;
        reply.append( buffer.data(), static_cast<size_t>( received ) );
    }
    CloseSocket( client );
#if defined( _WIN32 )
    WSACleanup();
#endif
    return reply;
}

string Post( string_view body, string_view origin = {} )
{
    string request = format( "POST /mcp HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Type: application/json\r\n"
                             "Accept: application/json, text/event-stream\r\nConnection: close\r\n"
                             "Content-Length: {}\r\n",
                             body.size() );
    if ( not origin.empty() )
        request += format( "Origin: {}\r\n", origin );
    return request + "\r\n" + string( body );
}

string BodyOf( const string& reply )
{
    const size_t end = reply.find( "\r\n\r\n" );
    return end == string::npos ? string() : reply.substr( end + 4 );
}
}

TEST_CASE( "The MCP protocol says what the server is, lists its tools, and calls them" )
{
    const McpProtocol protocol( vector<McpTool>{ Echo() }, "Say things." );
    const McpProtocol::Call call = []( const McpTool& tool, const Json& args ) { return tool.mTryCall( args ); };

    // The client's version, when the server speaks it.
    const Json hello = *protocol.Handle( Parse( R"({ "jsonrpc": "2.0", "id": 1, "method": "initialize",
                    "params": { "protocolVersion": "2025-06-18", "capabilities": {},
                                "clientInfo": { "name": "test", "version": "1" } } })" ),
                                         call );
    CHECK( *hello.TryFind( "id" )->TryAs<f64>() == 1 );
    const Json& result = *hello.TryFind( "result" );
    CHECK( *result.TryFind( "protocolVersion" )->TryAs<string>() == "2025-06-18" );
    CHECK( result.TryFind( "capabilities" )->TryFind( "tools" ) );
    CHECK( *result.TryFind( "serverInfo" )->TryFind( "name" )->TryAs<string>() == "bubble_editor" );
    CHECK( *result.TryFind( "instructions" )->TryAs<string>() == "Say things." );
    // One it does not: its own newest.
    const Json future = *protocol.Handle(
        Parse(
            R"({ "jsonrpc": "2.0", "id": 2, "method": "initialize", "params": { "protocolVersion": "2099-01-01" } })" ),
        call );
    CHECK( *future.TryFind( "result" )->TryFind( "protocolVersion" )->TryAs<string>() == "2025-11-25" );

    // A notification has no answer.
    CHECK_FALSE( protocol.Handle( Parse( R"({ "jsonrpc": "2.0", "method": "notifications/initialized" })" ), call ) );

    const Json listed = *protocol.Handle( Parse( R"({ "jsonrpc": "2.0", "id": "a", "method": "tools/list" })" ), call );
    CHECK( *listed.TryFind( "id" )->TryAs<string>() == "a" );
    const Json::Array& tools = *listed.TryFind( "result" )->TryFind( "tools" )->TryAs<Json::Array>();
    REQUIRE( tools.size() == 1 );
    CHECK( *tools[0].TryFind( "name" )->TryAs<string>() == "echo" );
    CHECK( tools[0].TryFind( "inputSchema" )->TryFind( "properties" ) );

    // A call answers with the tool's content; a tool's error is the tool's,
    // for the agent to read.
    const Json said = *protocol.Handle( Parse( R"({ "jsonrpc": "2.0", "id": 3, "method": "tools/call",
                    "params": { "name": "echo", "arguments": { "text": "hi" } } })" ),
                                        call );
    CHECK( TextOf( *said.TryFind( "result" ) ) == "hi" );
    CHECK_FALSE( Failed( *said.TryFind( "result" ) ) );
    const Json refused = *protocol.Handle(
        Parse( R"({ "jsonrpc": "2.0", "id": 4, "method": "tools/call", "params": { "name": "echo" } })" ), call );
    CHECK( Failed( *refused.TryFind( "result" ) ) );
    CHECK( TextOf( *refused.TryFind( "result" ) ) == "text: what to say expected" );

    // A tool or a method that is not there is the protocol's error.
    const Json unknown = *protocol.Handle(
        Parse( R"({ "jsonrpc": "2.0", "id": 5, "method": "tools/call", "params": { "name": "shout" } })" ), call );
    CHECK( *unknown.TryFind( "error" )->TryFind( "code" )->TryAs<f64>() == -32602 );
    CHECK( *unknown.TryFind( "error" )->TryFind( "message" )->TryAs<string>() == "no tool shout (tools: echo)" );
    const Json nothing =
        *protocol.Handle( Parse( R"({ "jsonrpc": "2.0", "id": 6, "method": "resources/list" })" ), call );
    CHECK( *nothing.TryFind( "error" )->TryFind( "code" )->TryAs<f64>() == -32601 );
    const Json pong = *protocol.Handle( Parse( R"({ "jsonrpc": "2.0", "id": 7, "method": "ping" })" ), call );
    CHECK( pong.TryFind( "result" )->Is<Json::Object>() );
}

TEST_CASE( "The editor's MCP tools read the shown scene and change it through the operators, one step each" )
{
    Project project;

    // The tree: paths, components, depth.
    const string tree = *project.Call( "scene_tree" );
    CHECK( tree == "/lamp  [transform, point_light]\n  /lamp/bulb  [transform]\n/floor  [transform]\n" );
    CHECK( *project.Call( "scene_tree", R"({ "depth": 1 })" ) ==
           "/lamp  [transform, point_light]\n  ... 1 under it\n/floor  [transform]\n" );
    CHECK( *project.Call( "scene_tree", R"({ "root": "/lamp" })" ) == "/lamp/bulb  [transform]\n" );
    CHECK( project.Call( "scene_tree", R"({ "root": "/sofa" })" ).error().starts_with( "root: " ) );

    // An entity as the file holds it.
    const Json lamp = Parse( *project.Call( "get_entity", R"({ "entity": "/lamp" })" ) );
    CHECK( *lamp.TryFind( "name" )->TryAs<string>() == "lamp" );
    CHECK( lamp.TryFind( "components" )->TryFind( "point_light" ) );

    // A field set as a file writes it: a step of the history, undone.
    CHECK(
        *project.Call( "set_field",
                       R"({ "entity": "/lamp", "component": "transform", "path": "position", "value": [4, 5, 6] })" ) ==
        "set" );
    CHECK( project.LampAt() == vec3( 4, 5, 6 ) );
    CHECK( project.mProject->GetWorkspace().TryActive()->IsDirty() );
    CHECK( *project.Call( "run_operator", R"({ "id": "edit.undo" })" ) == "done" );
    CHECK( project.LampAt() == vec3( 1, 2, 3 ) );
    CHECK( project
               .Call( "set_field",
                      R"({ "entity": "/lamp", "component": "transform", "path": "position", "value": "up" })" )
               .error()
               .find( "transform.position" ) != string::npos );
    CHECK( project.Call( "set_field", R"({ "entity": "/lamp", "component": "sofa", "path": "x", "value": 1 })" )
               .error()
               .starts_with( "component: sofa is not one (components: " ) );

    // Operators, listed and run; their errors said.
    CHECK( project.Call( "list_operators" )->find( "entity.rename - Rename: entity?: an entity, name: text" ) !=
           string::npos );
    CHECK( *project.Call( "run_operator",
                          R"({ "id": "entity.rename", "args": { "entity": "/floor", "name": "ground" } })" ) ==
           "done" );
    CHECK( project.Call( "scene_tree" )->find( "/ground" ) != string::npos );
    CHECK( project.Call( "run_operator", R"({ "id": "entity.rename", "args": { "entity": "/ground" } })" )
               .error()
               .starts_with( "entity.rename: " ) );

    // The editor's Luau: what it prints comes back, its error with it.
    CHECK( *project.Call( "run_lua", R"j({ "source": "print( editor.get( '/lamp', 'point_light.intensity' ) )" })j" ) ==
           "5\n" );
    CHECK( project.Call( "run_lua", R"j({ "source": "print( 'before' ) error( 'broken' )" })j" )
               .error()
               .starts_with( "before\n" ) );

    // The state: the level shown and active, the selection.
    REQUIRE( project.mProject->GetOperators().TryRun( project.mProject->GetWorkspace(), "selection.set",
                                                      Parse( R"({ "entities": ["/lamp"] })" ) ) );
    const Json state = Parse( *project.Call( "editor_state" ) );
    CHECK( *state.TryFind( "project" )->TryAs<string>() == "test" );
    const Json& level = state.TryFind( "documents" )->TryAs<Json::Array>()->front();
    CHECK( *level.TryFind( "path" )->TryAs<string>() == "levels/main.scene" );
    CHECK( *level.TryFind( "shown" )->TryAs<bool>() );
    CHECK( *state.TryFind( "active_entity" )->TryAs<string>() == "/lamp" );
    CHECK( *state.TryFind( "game" )->TryAs<string>() == "stopped" );

    // The project's files, and the log with what to ask from next.
    CHECK( project.Call( "list_assets" )->find( "levels/main.scene  level" ) != string::npos );
    CHECK( project.Call( "list_assets", R"({ "folder": "materials" })" ) == "no files there" );
    LogError( "a broken thing" );
    const string log = *project.Call( "read_log", R"({ "errors_only": true, "limit": 1 })" );
    CHECK( log.find( "error: a broken thing" ) != string::npos );
    CHECK( log.find( "next: " ) != string::npos );
}

TEST_CASE( "Pictures for agents: shrunk by averaging, carried as base64" )
{
    CHECK( EncodeBase64( "" ) == "" );
    CHECK( EncodeBase64( "f" ) == "Zg==" );
    CHECK( EncodeBase64( "fo" ) == "Zm8=" );
    CHECK( EncodeBase64( "foo" ) == "Zm9v" );
    CHECK( EncodeBase64( "foobar" ) == "Zm9vYmFy" );

    // Four pixels to one, their average.
    const vector<u8> pixels{ 0, 0, 0, 255, 100, 0, 0, 255, 0, 200, 0, 255, 0, 0, 40, 255 };
    const Rgba8Picture shrunk = ShrinkRgba8( pixels, ivec2( 2, 2 ), 1 );
    CHECK( shrunk.mSize == ivec2( 1, 1 ) );
    CHECK( shrunk.mPixels == vector<u8>{ 25, 50, 10, 255 } );
    // Small enough already: as it is.
    CHECK( ShrinkRgba8( pixels, ivec2( 2, 2 ), 800 ).mPixels == pixels );
    // The longer side to the size asked, the shorter in proportion.
    const vector<u8> wide( 400 * 100 * 4, 128 );
    CHECK( ShrinkRgba8( wide, ivec2( 400, 100 ), 200 ).mSize == ivec2( 200, 50 ) );
}

TEST_CASE( "The MCP server answers a client on this machine over HTTP, its tools run as the main thread serves" )
{
    auto serverMaybe = McpServer::TryStart( 0, vector<McpTool>{ Echo() }, "Say things." );
    REQUIRE_MESSAGE( serverMaybe, serverMaybe.error() );
    McpServer& server = **serverMaybe;
    CHECK( server.Port() != 0 );
    CHECK( server.Url() == format( "http://127.0.0.1:{}/mcp", server.Port() ) );

    // The client waits on its own thread while this one, the main one,
    // serves.
    const auto ask = [&]( string request ) {
        string reply;
        atomic<bool> done{ false };
        thread client( [&] {
            reply = Request( server.Port(), request );
            done = true;
        } );
        while ( not done )
        {
            server.Serve();
            this_thread::sleep_for( chrono::milliseconds( 1 ) );
        }
        client.join();
        return reply;
    };

    const string listed = ask( Post( R"({ "jsonrpc": "2.0", "id": 1, "method": "tools/list" })" ) );
    CHECK( listed.starts_with( "HTTP/1.1 200 OK\r\n" ) );
    CHECK( listed.find( "Content-Type: application/json" ) != string::npos );
    CHECK( BodyOf( listed ).find( "\"echo\"" ) != string::npos );

    const string said = ask( Post(
        R"({ "jsonrpc": "2.0", "id": 2, "method": "tools/call", "params": { "name": "echo", "arguments": { "text": "over the wire" } } })" ) );
    CHECK( TextOf( *Parse( BodyOf( said ) ).TryFind( "result" ) ) == "over the wire" );

    // A notification is accepted with nothing to say.
    CHECK( ask( Post( R"({ "jsonrpc": "2.0", "method": "notifications/initialized" })" ) )
               .starts_with( "HTTP/1.1 202 Accepted\r\n" ) );
    // A page of this machine may ask; a web page may not.
    CHECK( ask( Post( R"({ "jsonrpc": "2.0", "id": 3, "method": "ping" })", "http://localhost:3000" ) )
               .starts_with( "HTTP/1.1 200 " ) );
    CHECK( ask( Post( R"({ "jsonrpc": "2.0", "id": 4, "method": "ping" })", "https://example.com" ) )
               .starts_with( "HTTP/1.1 403 " ) );
    // No stream to open; nothing else served; JSON that does not parse said.
    CHECK( ask( "GET /mcp HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n" ).starts_with( "HTTP/1.1 405 " ) );
    CHECK( ask( "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n" ).starts_with( "HTTP/1.1 404 " ) );
    const string broken = ask( Post( "{ not json" ) );
    CHECK( broken.starts_with( "HTTP/1.1 400 " ) );
    CHECK( BodyOf( broken ).find( "-32700" ) != string::npos );

    // A second server on the same port is told it is taken.
    const auto twinMaybe = McpServer::TryStart( server.Port(), {}, "" );
    REQUIRE_FALSE( twinMaybe );
    CHECK( twinMaybe.error().starts_with( format( "MCP server: port {} is taken", server.Port() ) ) );
}
