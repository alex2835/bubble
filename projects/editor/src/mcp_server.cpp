#include "bubble/editor/mcp_server.hpp"
#include "bubble/core/log.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"

// The sockets' C API, last: Windows' headers define words as macros.
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
#include <cerrno>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace bubble
{
namespace
{
#if defined( _WIN32 )
using SocketHandle = SOCKET;
constexpr SocketHandle cNoSocket = INVALID_SOCKET;
void CloseSocket( SocketHandle socket )
{
    closesocket( socket );
}
string SocketError()
{
    return format( "error {}", WSAGetLastError() );
}
int Wait( vector<pollfd>& sockets, int milliseconds )
{
    return WSAPoll( sockets.data(), static_cast<ULONG>( sockets.size() ), milliseconds );
}
i64 Receive( SocketHandle socket, char* data, size_t size )
{
    return recv( socket, data, static_cast<int>( size ), 0 );
}
i64 Send( SocketHandle socket, const char* data, size_t size )
{
    return send( socket, data, static_cast<int>( size ), 0 );
}
#else
using SocketHandle = int;
constexpr SocketHandle cNoSocket = -1;
void CloseSocket( SocketHandle socket )
{
    close( socket );
}
string SocketError()
{
    return format( "error {}", errno );
}
int Wait( vector<pollfd>& sockets, int milliseconds )
{
    return poll( sockets.data(), static_cast<nfds_t>( sockets.size() ), milliseconds );
}
i64 Receive( SocketHandle socket, char* data, size_t size )
{
    return recv( socket, data, size, 0 );
}
// A client gone does not kill the editor with SIGPIPE.
i64 Send( SocketHandle socket, const char* data, size_t size )
{
    return send( socket, data, size, MSG_NOSIGNAL );
}
#endif

// What the server answers, the newest first; the oldest it still speaks.
constexpr array<string_view, 4> cProtocolVersions{ "2025-11-25", "2025-06-18", "2025-03-26", "2024-11-05" };
// A request this large, headers or body, is refused: an agent sends less.
constexpr size_t cLargestHeaders = 64 * 1024;
constexpr size_t cLargestBody = 16 * 1024 * 1024;
// How long the server waits for the main thread to run a tool.
constexpr auto cLongestCall = chrono::seconds( 60 );
// How long a connection may stay idle.
constexpr auto cLongestIdle = chrono::seconds( 120 );

Json Member( string_view key, Json value, Json::Object object = {} )
{
    object.push_back( Json::Member{ .mKey = string( key ), .mValue = std::move( value ) } );
    return Json( std::move( object ) );
}

Json Object( initializer_list<pair<string_view, Json>> members )
{
    Json::Object object;
    for ( const auto& [key, value] : members )
        object.push_back( Json::Member{ .mKey = string( key ), .mValue = value } );
    return Json( std::move( object ) );
}

Json Reply( const Json& id, Json result )
{
    return Object( { { "jsonrpc", Json( "2.0"s ) }, { "id", id }, { "result", std::move( result ) } } );
}

Json Failure( const Json& id, int code, string message )
{
    return Object( { { "jsonrpc", Json( "2.0"s ) },
                     { "id", id },
                     { "error", Object( { { "code", Json( static_cast<f64>( code ) ) },
                                          { "message", Json( std::move( message ) ) } } ) } } );
}

// JSON-RPC's codes.
constexpr int cParseError = -32700;
constexpr int cInvalidRequest = -32600;
constexpr int cMethodNotFound = -32601;
constexpr int cInvalidParams = -32602;

// An Origin of this machine: what a page of a server here sends. Any other
// is a web page's, which may not drive the editor.
bool LocalOrigin( string_view origin )
{
    for ( const string_view scheme : { "http://"sv, "https://"sv } )
        if ( origin.starts_with( scheme ) )
        {
            string_view host = origin.substr( scheme.size() );
            if ( host.starts_with( "[::1]" ) )
                return host.size() == 5 or host[5] == ':';
            host = host.substr( 0, host.find( ':' ) );
            return host == "localhost" or host == "127.0.0.1";
        }
    return false;
}

string_view StatusText( int status )
{
    switch ( status )
    {
        case 200: return "OK";
        case 202: return "Accepted";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 411: return "Length Required";
        case 413: return "Content Too Large";
        default: return "Error";
    }
}

string Lowered( string_view text )
{
    string lowered( text );
    for ( char& c : lowered )
        if ( c >= 'A' and c <= 'Z' )
            c = static_cast<char>( c - 'A' + 'a' );
    return lowered;
}
}

Json McpText( string text )
{
    return Object( { { "type", Json( "text"s ) }, { "text", Json( std::move( text ) ) } } );
}

// ---- the protocol --------------------------------------------------------------

McpProtocol::McpProtocol( vector<McpTool> tools, string instructions )
    : mTools( std::move( tools ) ),
      mInstructions( std::move( instructions ) )
{
}

opt<Json> McpProtocol::Handle( const Json& message, const Call& call ) const
{
    if ( not message.Is<Json::Object>() )
        return Failure( Json(), cInvalidRequest, "a JSON-RPC message is an object; batches are not taken" );
    const auto methodMaybe = message.TryFind( "method" );
    const auto idMaybe = message.TryFind( "id" );
    // A notification, or a client's answer to what the server never asks:
    // nothing to say.
    if ( not methodMaybe or not idMaybe )
        return nullopt;
    const Json& id = *idMaybe;
    const auto nameMaybe = methodMaybe->TryAs<string>();
    if ( not nameMaybe )
        return Failure( id, cInvalidRequest, "method: a text expected" );
    const string& method = *nameMaybe;
    const auto paramsMaybe = message.TryFind( "params" );
    const Json params = paramsMaybe ? *paramsMaybe : Json( Json::Object() );

    if ( method == "initialize" )
    {
        // The client's version when the server speaks it, else the newest.
        string version( cProtocolVersions.front() );
        if ( const auto askedMaybe = params.TryFind( "protocolVersion" ) )
            if ( const auto textMaybe = askedMaybe->TryAs<string>();
                 textMaybe and ranges::contains( cProtocolVersions, string_view( *textMaybe ) ) )
                version = *textMaybe;
        return Reply(
            id, Object( { { "protocolVersion", Json( version ) },
                          { "capabilities", Object( { { "tools", Object( { { "listChanged", Json( false ) } } ) } } ) },
                          { "serverInfo", Object( { { "name", Json( "bubble_editor"s ) },
                                                    { "title", Json( "Bubble editor"s ) },
                                                    { "version", Json( "0.1"s ) } } ) },
                          { "instructions", Json( mInstructions ) } } ) );
    }
    if ( method == "ping" )
        return Reply( id, Json( Json::Object() ) );
    if ( method == "tools/list" )
    {
        Json::Array tools;
        for ( const McpTool& tool : mTools )
            tools.push_back( Object( { { "name", Json( tool.mName ) },
                                       { "description", Json( tool.mDescription ) },
                                       { "inputSchema", tool.mInputSchema } } ) );
        return Reply( id, Member( "tools", Json( std::move( tools ) ) ) );
    }
    if ( method == "tools/call" )
    {
        const auto toolNameMaybe = params.TryFind( "name" );
        const auto toolTextMaybe = toolNameMaybe ? toolNameMaybe->TryAs<string>() : nullopt;
        if ( not toolTextMaybe )
            return Failure( id, cInvalidParams, "name: the tool's name expected" );
        const auto tool = ranges::find( mTools, *toolTextMaybe, &McpTool::mName );
        if ( tool == mTools.end() )
        {
            string names;
            for ( const McpTool& known : mTools )
                names += ( names.empty() ? "" : ", " ) + known.mName;
            return Failure( id, cInvalidParams, format( "no tool {} (tools: {})", *toolTextMaybe, names ) );
        }
        const auto argsMaybe = params.TryFind( "arguments" );
        const Json args = argsMaybe ? *argsMaybe : Json( Json::Object() );
        if ( not args.Is<Json::Object>() )
            return Failure( id, cInvalidParams, "arguments: an object expected" );
        auto contentMaybe = call( *tool, args );
        const bool failed = not contentMaybe;
        Json::Array content =
            failed ? Json::Array{ McpText( std::move( contentMaybe.error() ) ) } : std::move( *contentMaybe );
        return Reply( id, Object( { { "content", Json( std::move( content ) ) }, { "isError", Json( failed ) } } ) );
    }
    return Failure( id, cMethodNotFound, format( "no method {}", method ) );
}

// ---- the server ----------------------------------------------------------------

// A tool's call, handed from the server's thread to the main one.
struct McpServer::Pending
{
    const McpTool& mTool;
    Json mArgs;
    opt<expected<Json::Array, string>> mResultMaybe;
    // The server's thread gave up waiting: not to run.
    bool mAbandoned = false;
};

// The sockets: the one listening, and the connections it took.
struct McpServer::Listener
{
    struct Connection
    {
        SocketHandle mSocket = cNoSocket;
        string mReceived;
        chrono::steady_clock::time_point mLastHeard;
    };

    SocketHandle mSocket = cNoSocket;
    vector<Connection> mConnections;
#if defined( _WIN32 )
    bool mStarted = false;
#endif

    Listener() = default;
    Listener( const Listener& ) = delete;
    Listener& operator=( const Listener& ) = delete;

    ~Listener()
    {
        for ( const Connection& connection : mConnections )
            CloseSocket( connection.mSocket );
        if ( mSocket != cNoSocket )
            CloseSocket( mSocket );
#if defined( _WIN32 )
        if ( mStarted )
            WSACleanup();
#endif
    }

    expected<u16, string> TryListen( u16 port )
    {
#if defined( _WIN32 )
        WSADATA data{};
        if ( WSAStartup( MAKEWORD( 2, 2 ), &data ) != 0 )
            return unexpected( "the sockets did not start: " + SocketError() );
        mStarted = true;
#endif
        mSocket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
        if ( mSocket == cNoSocket )
            return unexpected( "no socket: " + SocketError() );
#if defined( _WIN32 )
        // No other process may take the port while the editor has it.
        const BOOL exclusive = TRUE;
        setsockopt( mSocket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>( &exclusive ),
                    sizeof( exclusive ) );
#else
        // An editor started again takes its port back at once.
        const int reuse = 1;
        setsockopt( mSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof( reuse ) );
#endif
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons( port );
        address.sin_addr.s_addr = htonl( INADDR_LOOPBACK );
        if ( bind( mSocket, reinterpret_cast<const sockaddr*>( &address ), sizeof( address ) ) != 0 )
            return unexpected(
                format( "port {} is taken: another editor or program listens there ({})", port, SocketError() ) );
        if ( listen( mSocket, 8 ) != 0 )
            return unexpected( "does not listen: " + SocketError() );
        socklen_t length = sizeof( address );
        getsockname( mSocket, reinterpret_cast<sockaddr*>( &address ), &length );
        return ntohs( address.sin_port );
    }

    // Waits a tenth of a second at most for what there is to read: a
    // connection taken, what came read, each request whole answered.
    void Poll( const function<pair<int, string>( string_view, string_view, string_view, string_view )>& answer )
    {
        // The listening socket first, then each connection's, in order.
        vector<pollfd> sockets;
        sockets.push_back( pollfd{ .fd = mSocket, .events = POLLIN, .revents = 0 } );
        for ( const Connection& connection : mConnections )
            sockets.push_back( pollfd{ .fd = connection.mSocket, .events = POLLIN, .revents = 0 } );
        if ( Wait( sockets, 100 ) <= 0 )
        {
            DropIdle();
            return;
        }
        const auto heard = [&]( size_t at ) { return ( sockets[at].revents & ( POLLIN | POLLHUP | POLLERR ) ) != 0; };
        // Read before a new connection is taken: the order is the poll's.
        for ( size_t at = 0; at < mConnections.size(); ++at )
        {
            Connection& connection = mConnections[at];
            if ( not heard( at + 1 ) )
                continue;
            array<char, 16384> buffer{};
            const i64 received = Receive( connection.mSocket, buffer.data(), buffer.size() );
            if ( received <= 0 )
            {
                Close( connection );
                continue;
            }
            connection.mReceived.append( buffer.data(), static_cast<size_t>( received ) );
            connection.mLastHeard = chrono::steady_clock::now();
            while ( connection.mSocket != cNoSocket and Answered( connection, answer ) )
            {
            }
        }
        if ( heard( 0 ) )
        {
            const SocketHandle taken = accept( mSocket, nullptr, nullptr );
            if ( taken != cNoSocket )
                mConnections.push_back(
                    Connection{ .mSocket = taken, .mReceived = {}, .mLastHeard = chrono::steady_clock::now() } );
        }
        erase_if( mConnections, []( const Connection& connection ) { return connection.mSocket == cNoSocket; } );
        DropIdle();
    }

    // The first request a connection has whole, answered and taken off what
    // came; false while it is not whole yet.
    static bool Answered(
        Connection& connection,
        const function<pair<int, string>( string_view, string_view, string_view, string_view )>& answer )
    {
        const size_t headersEnd = connection.mReceived.find( "\r\n\r\n" );
        if ( headersEnd == string::npos )
        {
            if ( connection.mReceived.size() > cLargestHeaders )
                Respond( connection, 413, {}, false );
            return false;
        }
        const string_view head = string_view( connection.mReceived ).substr( 0, headersEnd );
        const size_t lineEnd = head.find( "\r\n" );
        const string_view requestLine = head.substr( 0, lineEnd );
        const size_t space = requestLine.find( ' ' );
        const size_t secondSpace = requestLine.find( ' ', space + 1 );
        if ( space == string_view::npos or secondSpace == string_view::npos )
        {
            Respond( connection, 400, {}, false );
            return false;
        }
        const string method( requestLine.substr( 0, space ) );
        const string target( requestLine.substr( space + 1, secondSpace - space - 1 ) );
        string origin;
        opt<size_t> lengthMaybe;
        bool keepAlive = true;
        bool chunked = false;
        for ( size_t at = lineEnd == string_view::npos ? head.size() : lineEnd + 2; at < head.size(); )
        {
            const size_t end = min( head.find( "\r\n", at ), head.size() );
            const string_view line = head.substr( at, end - at );
            at = end + 2;
            const size_t colon = line.find( ':' );
            if ( colon == string_view::npos )
                continue;
            const string name = Lowered( line.substr( 0, colon ) );
            string_view value = line.substr( colon + 1 );
            while ( not value.empty() and value.front() == ' ' )
                value.remove_prefix( 1 );
            if ( name == "content-length" )
                lengthMaybe = TryParse<size_t>( value );
            else if ( name == "origin" )
                origin = value;
            else if ( name == "connection" )
                keepAlive = Lowered( value ) != "close";
            else if ( name == "transfer-encoding" )
                chunked = true;
        }
        if ( chunked or ( method == "POST" and not lengthMaybe ) )
        {
            Respond( connection, 411, {}, false );
            return false;
        }
        const size_t length = lengthMaybe.value_or( 0 );
        if ( length > cLargestBody )
        {
            Respond( connection, 413, {}, false );
            return false;
        }
        const size_t bodyStart = headersEnd + 4;
        if ( connection.mReceived.size() < bodyStart + length )
            return false;
        const string body = connection.mReceived.substr( bodyStart, length );
        connection.mReceived.erase( 0, bodyStart + length );
        const auto [status, reply] = answer( method, target, origin, body );
        Respond( connection, status, reply, keepAlive );
        return true;
    }

    static void Respond( Connection& connection, int status, string_view body, bool keepAlive )
    {
        string response = format( "HTTP/1.1 {} {}\r\nContent-Length: {}\r\nConnection: {}\r\n", status,
                                  StatusText( status ), body.size(), keepAlive ? "keep-alive" : "close" );
        if ( not body.empty() )
            response += "Content-Type: application/json\r\n";
        if ( status == 405 )
            response += "Allow: POST\r\n";
        response += "\r\n";
        response += body;
        for ( size_t sent = 0; sent < response.size(); )
        {
            const i64 wrote = Send( connection.mSocket, response.data() + sent, response.size() - sent );
            if ( wrote <= 0 )
            {
                keepAlive = false;
                break;
            }
            sent += static_cast<size_t>( wrote );
        }
        if ( not keepAlive )
            Close( connection );
    }

    static void Close( Connection& connection )
    {
        if ( connection.mSocket != cNoSocket )
            CloseSocket( connection.mSocket );
        connection.mSocket = cNoSocket;
    }

    void DropIdle()
    {
        const auto now = chrono::steady_clock::now();
        for ( Connection& connection : mConnections )
            if ( now - connection.mLastHeard > cLongestIdle )
                Close( connection );
        erase_if( mConnections, []( const Connection& connection ) { return connection.mSocket == cNoSocket; } );
    }
};

expected<Scope<McpServer>, string> McpServer::TryStart( u16 port, vector<McpTool> tools, string instructions )
{
#if defined( __EMSCRIPTEN__ )
    return unexpected( "the MCP server is the desktop's alone"s );
#else
    auto listener = CreateScope<Listener>();
    auto portMaybe = listener->TryListen( port );
    if ( not portMaybe )
        return unexpected( format( "MCP server: {}", portMaybe.error() ) );
    return Scope<McpServer>( new McpServer( McpProtocol( std::move( tools ), std::move( instructions ) ),
                                            std::move( listener ), *portMaybe ) );
#endif
}

McpServer::McpServer( McpProtocol protocol, Scope<Listener> listener, u16 port )
    : mProtocol( std::move( protocol ) ),
      mListener( std::move( listener ) ),
      mPort( port )
{
    mThread = thread( [this] { Run(); } );
}

McpServer::~McpServer()
{
    mStopping = true;
    mAnswered.notify_all();
    if ( mThread.joinable() )
        mThread.join();
}

string McpServer::Url() const
{
    return format( "http://127.0.0.1:{}/mcp", mPort );
}

void McpServer::Run()
{
    while ( not mStopping )
        mListener->Poll( [this]( string_view method, string_view target, string_view origin, string_view body ) {
            return Answer( method, target, origin, body );
        } );
}

pair<int, string> McpServer::Answer( string_view method, string_view target, string_view origin, string_view body )
{
    if ( target.substr( 0, target.find( '?' ) ) != "/mcp" )
        return pair<int, string>{ 404, {} };
    if ( not origin.empty() and not LocalOrigin( origin ) )
        return pair<int, string>{ 403, {} };
    // No stream to open, no session to end: POST alone.
    if ( method != "POST" )
        return pair<int, string>{ 405, {} };
    auto messageMaybe = Json::TryParse( body );
    if ( not messageMaybe )
        return pair<int, string>{ 400, Failure( Json(), cParseError, messageMaybe.error() ).Write() };
    const auto answerMaybe = mProtocol.Handle(
        *messageMaybe, [this]( const McpTool& tool, const Json& args ) { return CallOnMain( tool, args ); } );
    if ( not answerMaybe )
        return pair<int, string>{ 202, {} };
    return pair<int, string>{ 200, answerMaybe->Write() };
}

expected<Json::Array, string> McpServer::CallOnMain( const McpTool& tool, const Json& args )
{
    const auto pending = CreateRef<Pending>( Pending{ .mTool = tool, .mArgs = args, .mResultMaybe = {} } );
    unique_lock lock( mMutex );
    mPending.push_back( pending );
    const bool answered =
        mAnswered.wait_for( lock, cLongestCall, [&] { return pending->mResultMaybe.has_value() or mStopping.load(); } );
    if ( pending->mResultMaybe )
        return std::move( *pending->mResultMaybe );
    pending->mAbandoned = true;
    if ( not answered )
        return unexpected( format( "the editor did not run {} in {} s: it is busy, or a dialog waits", tool.mName,
                                   cLongestCall.count() ) );
    return unexpected( "the editor is closing"s );
}

void McpServer::Serve()
{
    vector<Ref<Pending>> pending;
    {
        const lock_guard lock( mMutex );
        pending.swap( mPending );
    }
    for ( const Ref<Pending>& call : pending )
    {
        {
            const lock_guard lock( mMutex );
            if ( call->mAbandoned )
                continue;
        }
        auto resultMaybe = call->mTool.mTryCall( call->mArgs );
        const lock_guard lock( mMutex );
        call->mResultMaybe = std::move( resultMaybe );
    }
    mAnswered.notify_all();
}
}
