#pragma once
#include "bubble/data/json.hpp"
#include "bubble/types/function.hpp"
#include "bubble/types/pointer.hpp"
#include "bubble/types/thread.hpp"

// The editor's MCP server: an agent - Claude Code, any client of the Model
// Context Protocol - reads and changes what the running editor has open.
// It speaks Streamable HTTP on 127.0.0.1 alone, answering each request with
// one JSON reply, no stream; a request from a web page - an Origin that is
// not this machine's - is refused. Its own thread reads and writes; the
// tools run on the editor's main thread, between frames, as the menus do.
namespace bubble
{
// A tool an agent calls: its name, what it does and what it takes - a JSON
// schema -, as the agent reads them, and what it runs.
struct McpTool
{
    string mName;
    string mDescription;
    Json mInputSchema;
    // The arguments - an object - to what the agent reads back: MCP's
    // content, { "type": "text", "text": ... } and images. An error is told
    // the agent as the tool's, not the protocol's: it can try again.
    function<expected<Json::Array, string>( const Json& args )> mTryCall;
};

// A text content, as a tool answers.
Json McpText( string text );

// The protocol apart from how its messages come: one JSON-RPC message in,
// the answer out - nothing for a notification, or a client's answer.
class McpProtocol
{
public:
    // Runs a tool where tools run: on the main thread, for the server; at
    // once, for a test.
    using Call = function<expected<Json::Array, string>( const McpTool& tool, const Json& args )>;

    // `instructions`: what the agent is told of the tools as a whole.
    McpProtocol( vector<McpTool> tools, string instructions );

    opt<Json> Handle( const Json& message, const Call& call ) const;
    span<const McpTool> Tools() const { return mTools; }

private:
    vector<McpTool> mTools;
    string mInstructions;
};

class McpServer
{
public:
    // Listens on 127.0.0.1 at `port` - 0 for any the system gives, as a test
    // wants -; an error that says why it cannot.
    static expected<Scope<McpServer>, string> TryStart( u16 port, vector<McpTool> tools, string instructions );
    // Stops listening: what is waiting is told the editor is closing.
    ~McpServer();
    McpServer( const McpServer& ) = delete;
    McpServer& operator=( const McpServer& ) = delete;

    // On the main thread, between frames: the calls that came in, run, and
    // their answers handed back.
    void Serve();

    u16 Port() const { return mPort; }
    // What a client connects to: "http://127.0.0.1:8752/mcp".
    string Url() const;

    // The port the editor listens on unless told another.
    static constexpr u16 cDefaultPort = 8752;

private:
    struct Pending;
    struct Listener;

    McpServer( McpProtocol protocol, Scope<Listener> listener, u16 port );
    void Run();
    // On the server's thread: a tool run on the main thread, waited for.
    expected<Json::Array, string> CallOnMain( const McpTool& tool, const Json& args );
    // A request's body to the reply: its status, and its body if any.
    pair<int, string> Answer( string_view method, string_view target, string_view origin, string_view body );

    McpProtocol mProtocol;
    Scope<Listener> mListener;
    u16 mPort = 0;
    atomic<bool> mStopping{ false };
    mutex mMutex;
    condition_variable mAnswered;
    vector<Ref<Pending>> mPending;
    thread mThread;
};
}
