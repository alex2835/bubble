#include "engine/pch/pch.hpp"
#include "engine/loader/loader.hpp"
#include "engine/loader/shader_loader_error_handling.hpp"
#include "engine/types/string.hpp"
#include <GL/glew.h>

namespace bubble
{
// GL reports the info log length including its null terminator. Resizing to
// that and stopping leaves the terminator inside the string, which is the stray
// blank line every shader error has printed.
string ShaderInfoLog( u32 shaderId )
{
	i32 length = 0;
	glGetShaderiv( shaderId, GL_INFO_LOG_LENGTH, &length );
	if ( length <= 0 )
		return {};

	string log;
	log.resize( length );
	i32 written = 0;
	glGetShaderInfoLog( shaderId, length, &written, log.data() );
	log.resize( written );
	return log;
}

// The same, for a program object. This used to be done with glGetShaderiv on
// the program name - which is GL_INVALID_OPERATION, leaves the length at zero
// and asks glGetProgramInfoLog for no bytes at all, so every link failure so
// far has reported an empty log.
string ProgramInfoLog( u32 programId )
{
	i32 length = 0;
	glGetProgramiv( programId, GL_INFO_LOG_LENGTH, &length );
	if ( length <= 0 )
		return {};

	string log;
	log.resize( length );
	i32 written = 0;
	glGetProgramInfoLog( programId, length, &written, log.data() );
	log.resize( written );
	return log;
}


// A shader's path is extensionless: it names a .vert/.frag pair, not a file
// anyone can open. Point at the stage that actually failed.
path StagePath( const Shader& shader, string_view extension )
{
	path stagePath = shader.mPath;
	stagePath.replace_extension( extension );
	// A shader with no vertex stage of its own compiles the engine's, so that
	// is the file to name when the vertex stage is the one that failed.
	if ( extension == ".vert" and not filesystem::exists( stagePath ) )
		return DEFAULT_VERTEX_SHADER;
	return stagePath;
}

// Driver logs end with a newline, and one blank line per error adds up fast
// when a single mistake produces a dozen of them.
static string_view TrimTrailing( string_view text )
{
	const auto end = text.find_last_not_of( " \t\r\n" );
	return end == string_view::npos ? string_view{} : text.substr( 0, end + 1 );
}


// A shader longer than this is a parse gone wrong, not a shader. Only here so
// a malformed log cannot spin the digit loop up into an overflow.
constexpr u32 cMaxSourceLines = 1000000;

// The line a driver log line refers to, when it names one. NVIDIA writes
// "0(231) : error C1104: ...", AMD "ERROR: 0:231: ...", Mesa "0:231(12): error".
// All three lead with a source-string index and a line, and GL is handed
// exactly one source string per stage, so in every case the wanted number is
// the one just past the first separator.
static std::optional<u32> LogLineNumber( string_view logLine )
{
	constexpr string_view cDigits = "0123456789"sv;

	const auto indexBegin = logLine.find_first_of( cDigits );
	if ( indexBegin == string_view::npos )
		return std::nullopt;

	const auto separator = logLine.find_first_not_of( cDigits, indexBegin );
	if ( separator == string_view::npos )
		return std::nullopt;
	if ( logLine[separator] != '(' and logLine[separator] != ':' )
		return std::nullopt;

	const size_t lineBegin = separator + 1;
	size_t cursor = lineBegin;
	u32 line = 0;
	while ( cursor < logLine.size() and cDigits.find( logLine[cursor] ) != string_view::npos )
	{
		line = line * 10 + u32( logLine[cursor] - '0' );
		if ( line > cMaxSourceLines )
			return std::nullopt;
		cursor++;
	}
	if ( cursor == lineBegin )
		return std::nullopt;
	return line;
}


// How much source to show either side of a fault.
constexpr u32 cExcerptContext = 2;

// The source around a fault, taken from the text that was compiled rather than
// re-read from disk. A hot reload fires because the file just changed, so what
// is on disk may already be a keystroke ahead of what failed; GL numbered these
// exact lines, and an excerpt cut from them cannot drift out of step with the
// log it sits under.
static string SourceExcerpt( string_view source, u32 faultLine )
{
	const u32 first = faultLine > cExcerptContext ? faultLine - cExcerptContext : 1;
	const u32 last = faultLine + cExcerptContext;

	string excerpt;
	size_t begin = 0;
	for ( u32 number = 1; number <= last; number++ )
	{
		// The text ends with a newline, which is not a further line of source.
		if ( begin >= source.size() and number > 1 )
			break;

		const auto newline = source.find( '\n', begin );
		const auto end = newline == string_view::npos ? source.size() : newline;

		if ( number >= first )
			excerpt += std::format( "  {} {:>4} | {}\n",
									number == faultLine ? ">" : " ",
									number,
									TrimTrailing( source.substr( begin, end - begin ) ) );

		if ( newline == string_view::npos )
			break;
		begin = newline + 1;
	}
	return excerpt;
}


// At most this many excerpts, so one mistake in a module does not print the
// same neighbourhood forty times over.
constexpr u32 cMaxExcerpts = 5;

// The driver log with the source it refers to interleaved under each error.
// Lines carrying no location pass through untouched, so nothing the driver said
// is ever dropped.
static string AnnotateLog( string_view log, StageSources sources )
{
	string annotated;
	u32 excerpts = 0;
	u32 lastLine = 0;
	bool capped = false;

	size_t begin = 0;
	while ( true )
	{
		const auto newline = log.find( '\n', begin );
		const auto end = newline == string_view::npos ? log.size() : newline;
		const string_view logLine = TrimTrailing( log.substr( begin, end - begin ) );

		if ( not logLine.empty() )
		{
			annotated += std::format( "  {}\n", logLine );

			if ( logLine.starts_with( "Vertex info" ) )
				sources.mActive = sources.mVertex;
			else if ( logLine.starts_with( "Fragment info" ) )
				sources.mActive = sources.mFragment;
			else if ( logLine.starts_with( "Geometry info" ) )
				sources.mActive = sources.mGeometry;

			const auto faultLine = LogLineNumber( logLine );
			// A second error on the same line does not want the same five lines
			// printed under it again.
			if ( faultLine and *faultLine != lastLine and not sources.mActive.empty() )
			{
				if ( excerpts < cMaxExcerpts )
				{
					annotated += SourceExcerpt( sources.mActive, *faultLine );
					lastLine = *faultLine;
					excerpts++;
				}
				else
					capped = true;
			}
		}

		if ( newline == string_view::npos )
			break;
		begin = newline + 1;
	}

	if ( capped )
		annotated += "  (further errors left unquoted - read them against the expanded source)\n";
	return annotated;
}


// The expanded stages, written out whole. An error two hundred lines into a
// file nobody wrote is only readable against the text the driver actually saw,
// and a cascade of them is not readable from excerpts at all. Returns where
// they went, or an empty path when they could not be written - failing to dump
// is not itself worth an error on top of the one being reported.
static path DumpExpandedSources( const Shader& shader, const StageSources& sources )
{
	std::error_code error;
	const path dir = filesystem::resolveNearExecutable( "shader_errors" );
	filesystem::create_directories( dir, error );
	if ( error )
		return {};

	const auto write = [&]( string_view extension, string_view text )
	{
		if ( text.empty() )
			return;
		std::ofstream file( dir / ( shader.mName + string( extension ) ),
							std::ios::binary | std::ios::trunc );
		if ( file )
			file.write( text.data(), (std::streamsize)text.size() );
	};
	write( ".vert", sources.mVertex );
	write( ".frag", sources.mFragment );
	write( ".geom", sources.mGeometry );
	return dir;
}


// One place, so the four failure paths cannot drift apart.
[[noreturn]] void ThrowShaderError( const Shader& shader,
										   string_view what,
										   const path& file,
										   string_view log,
										   const StageSources& sources )
{
	string report = std::format( "Shader '{}': {}.\n  {}\n{}",
								 shader.mName, what, file.string(),
								 AnnotateLog( log, sources ) );

	if ( const path dump = DumpExpandedSources( shader, sources ); not dump.empty() )
		report += std::format( "  expanded source written to {}", dump.string() );

	// The log goes to the console because that is where it is read. The
	// exception carries the shader's identity because at project open nothing
	// catches it and only what() survives - it used to be the bare string
	// "Shader compilation failed", naming nothing at all.
	LogError( "{}", TrimTrailing( report ) );
	throw std::runtime_error( std::format( "Shader '{}': {} ({})",
										   shader.mName, what, file.string() ) );
}

}

