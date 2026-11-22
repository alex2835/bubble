#pragma once
#include "bubble/editor/mcp_server.hpp"
#include "bubble/types/math.hpp"

namespace bubble
{
class EditorProject;
class Gpu;
class Viewport;

// What the editor's MCP server offers an agent, on the project open: its
// state - documents, selection, the game -, the shown scene's tree and an
// entity as a file holds it, a field set as a file writes it, the operators
// listed and run, the editor's Luau, the project's files and the log. Every
// change goes through the operators, one step of the history as a menu's.
vector<McpTool> EditorMcpTools( EditorProject& project );
// A picture of the viewport as it was last drawn, as a PNG at most
// `max_size` pixels across.
McpTool ViewportScreenshotTool( Gpu& gpu, const Viewport& viewport );
// What the agent is told of the tools as a whole.
string EditorMcpInstructions();

// The bytes as base64, as JSON carries an image.
string EncodeBase64( string_view bytes );
struct Rgba8Picture
{
    vector<u8> mPixels;
    ivec2 mSize{ 0 };
};
// RGBA8 pixels `size` across made smaller, by averaging, until neither side
// is longer than `longest`; as they are when they fit.
Rgba8Picture ShrinkRgba8( span<const u8> pixels, ivec2 size, i32 longest );
}
