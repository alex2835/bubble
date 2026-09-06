#include "engine/pch/pch.hpp"
#include "engine/loader/shader_module_loader.hpp"
#include "engine/types/string.hpp"

namespace bubble
{
// An include cycle would otherwise recurse until the stack runs out, and a
// mistyped shader is not worth taking the editor down over.
constexpr int cMaxImportDepth = 32;

// Set by Project::Open, so a project's shaders see its own modules.
static path sProjectShaderModulesDir;

void SetProjectShaderModulesDir( const path& dir )
{
	sProjectShaderModulesDir = dir;
}

const path& GetProjectShaderModulesDir()
{
	return sProjectShaderModulesDir;
}

vector<path> ShaderModuleSearchDirs()
{
	vector<path> dirs;
	// Project first: a project shadowing an engine module is a deliberate
	// override, and searching the engine first would silently ignore it.
	if ( not sProjectShaderModulesDir.empty() and filesystem::exists( sProjectShaderModulesDir ) )
		dirs.push_back( sProjectShaderModulesDir );
	dirs.emplace_back( SHADER_MODULES_SEARCH_PATH );
	return dirs;
}


ShaderModuleTable GetShaderModules()
{
	ShaderModuleTable modules;
	for ( const path& searchPath : ShaderModuleSearchDirs() )
	{
		std::error_code error;
		for ( const path& file : filesystem::directory_iterator( searchPath, error ) )
		{
			if ( file.extension() != MODULE_EXTENSION )
				continue;

			// emplace, not assignment: the first directory holding a module
			// wins, which is what lets a project shadow an engine one.
			modules.emplace( file.stem().string(),
							 ShaderModuleSource{ .mText = filesystem::readFile( file ),
												 .mDir = file.parent_path() } );
		}
	}
	return modules;
}


static string_view TrimLeft( string_view text )
{
	const auto begin = text.find_first_not_of( " \t" );
	return begin == string_view::npos ? string_view{} : text.substr( begin );
}

static string_view Trim( string_view text )
{
	const string_view left = TrimLeft( text );
	const auto end = left.find_last_not_of( " \t\r" );
	return end == string_view::npos ? string_view{} : left.substr( 0, end + 1 );
}


// The name in a `use name;` or `mod a::b;` directive - whatever sits between
// the keyword and the semicolon.
//
// The semicolon is required, as it is in Rust. A directive without one is a
// typo rather than a line of WGSL, and saying so beats passing `use common`
// through to the compiler and reporting it as a syntax error in generated
// source the author never wrote.
static string_view DirectiveName( string_view line, string_view keyword )
{
	const string_view rest = TrimLeft( line.substr( keyword.size() ) );

	const auto semicolon = rest.find( ';' );
	if ( semicolon == string_view::npos )
		throw std::runtime_error( std::format( "`{}` needs a terminating ';': {}", keyword, line ) );

	const string_view name = Trim( rest.substr( 0, semicolon ) );
	if ( name.empty() )
		throw std::runtime_error( std::format( "`{}` needs a name: {}", keyword, line ) );

	// A comment after the semicolon is fine; anything else is a mistake worth
	// naming rather than dropping on the floor.
	const string_view tail = Trim( rest.substr( semicolon + 1 ) );
	if ( not tail.empty() and not tail.starts_with( "//" ) )
		throw std::runtime_error( std::format( "unexpected text after ';': {}", line ) );

	return name;
}


// `mod lighting::util;` names the file lighting/util.wgsl, the way Rust's
// `mod util;` names util.rs.
static path ModuleFilePath( string_view modPath )
{
	string relative( modPath );
	for ( size_t pos = relative.find( "::" ); pos != string::npos; pos = relative.find( "::", pos + 1 ) )
		relative.replace( pos, 2, "/" );
	relative += MODULE_EXTENSION;
	return path( relative );
}


static ProcessedSource ProcessSource( string_view source,
									  const path& sourceDir,
									  const ShaderModuleTable& modules,
									  int depth );

static ProcessedSource ProcessFile( const path& shaderPath,
									const ShaderModuleTable& modules,
									int depth );


// A registered module, by name, from one of the module directories.
static ProcessedSource SpliceModule( string_view moduleName,
									 const ShaderModuleTable& modules,
									 int depth )
{
	const auto iter = modules.find( string( moduleName ) );
	if ( iter == modules.end() )
		throw std::runtime_error( std::format( "No such module name {}", moduleName ) );

	ProcessedSource spliced = ProcessSource( iter->second.mText, iter->second.mDir, modules, depth + 1 );

	const auto enumValue = magic_enum::enum_cast<ShaderModule>( moduleName, magic_enum::case_insensitive );
	spliced.mModules.set( enumValue.value_or( ShaderModule::None ) );
	return spliced;
}


// A file beside the one the directive is written in. Registering a module is
// the right call for an engine-owned interface and the wrong one for a user
// splitting their own shader in two.
static ProcessedSource SpliceRelativeFile( const path& relative,
										   const path& sourceDir,
										   const ShaderModuleTable& modules,
										   int depth )
{
	const path included = sourceDir / relative;
	if ( not filesystem::exists( included ) )
		throw std::runtime_error( std::format( "No such included file: {}", included.string() ) );

	return ProcessFile( included, modules, depth + 1 );
}


// One line: itself, or whatever it imports.
//
// WGSL is Rust shaped, so the directives are too:
//
//   use common;            a module registered in a module directory
//   mod helpers;           helpers.wgsl, beside this file
//   mod lighting::util;    lighting/util.wgsl, beside this file
//
// Neither keyword can collide with the shader's own code: these lines are
// consumed here and never reach the WGSL compiler, and no top level WGSL
// declaration begins with `use` or `mod` in the first place - they start with
// struct, fn, const, var, alias, override, enable, requires or an attribute.
static ProcessedSource ProcessLine( string_view rawLine,
									const path& sourceDir,
									const ShaderModuleTable& modules,
									int depth )
{
	const string_view line = TrimLeft( rawLine );

	const bool isUse = line.starts_with( "use " ) or line.starts_with( "use\t" );
	const bool isMod = line.starts_with( "mod " ) or line.starts_with( "mod\t" );

	if ( not isUse and not isMod )
	{
		// #include was the previous spelling. Letting it through as plain text
		// would hand the WGSL compiler a line it cannot parse, reported against
		// generated source the author never wrote, so name it here instead.
		if ( line.starts_with( "#include" ) )
			throw std::runtime_error( std::format(
				"`#include` is no longer supported - write `use name;` for a module "
				"or `mod path;` for a neighbouring file: {}", line ) );

		ProcessedSource plain;
		plain.mText = string( rawLine ) + '\n';
		return plain;
	}

	if ( depth >= cMaxImportDepth )
		throw std::runtime_error( std::format( "imports nested more than {} deep - cycle in {}?",
											   cMaxImportDepth, sourceDir.string() ) );

	if ( isUse )
		return SpliceModule( DirectiveName( line, "use" ), modules, depth );

	// isMod - the only case left.
	return SpliceRelativeFile( ModuleFilePath( DirectiveName( line, "mod" ) ),
							   sourceDir, modules, depth );

}


// `sourceDir` is where this text came from, and is what its own relative
// includes resolve against.
static ProcessedSource ProcessSource( string_view source,
									  const path& sourceDir,
									  const ShaderModuleTable& modules,
									  int depth )
{
	ProcessedSource processed;
	std::istringstream stream{ string( source ) };
	for ( string line; std::getline( stream, line ); )
	{
		ProcessedSource piece = ProcessLine( line, sourceDir, modules, depth );
		processed.mText += piece.mText;
		processed.mModules |= piece.mModules;
	}
	return processed;
}


static ProcessedSource ProcessFile( const path& shaderPath,
									const ShaderModuleTable& modules,
									int depth )
{
	return ProcessSource( filesystem::readFile( shaderPath ), shaderPath.parent_path(), modules, depth );
}


ProcessedSource ExpandShaderImports( const path& shaderPath, const ShaderModuleTable& modules )
{
	return ProcessFile( shaderPath, modules, 0 );
}

}

