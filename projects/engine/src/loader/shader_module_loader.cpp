#include "engine/pch/pch.hpp"
#include "engine/loader/shader_module_loader.hpp"
#include "engine/types/string.hpp"

namespace bubble
{
// An include cycle would otherwise recurse until the stack runs out, and a
// mistyped shader is not worth taking the editor down over.
constexpr int cMaxIncludeDepth = 32;

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
			if ( file.extension() != ".glsl" )
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


// The text between the first matching pair of delimiters, empty when the line
// has no such pair.
static string_view IncludeArgument( string_view line, char open, char close )
{
	const auto begin = line.find( open );
	if ( begin == string_view::npos )
		return {};
	const auto end = line.find( close, begin + 1 );
	if ( end == string_view::npos )
		return {};
	return line.substr( begin + 1, end - begin - 1 );
}


static ProcessedSource ProcessSource( string_view source,
									  const path& sourceDir,
									  const ShaderModuleTable& modules,
									  int depth );

static ProcessedSource ProcessFile( const path& shaderPath,
									const ShaderModuleTable& modules,
									int depth );


// One line: itself, or whatever it includes.
static ProcessedSource ProcessLine( string_view line,
									const path& sourceDir,
									const ShaderModuleTable& modules,
									int depth )
{
	if ( not line.starts_with( "#include" ) )
	{
		ProcessedSource plain;
		plain.mText = string( line ) + '\n';
		return plain;
	}

	if ( depth >= cMaxIncludeDepth )
		throw std::runtime_error( std::format( "#include nested more than {} deep - cycle in {}?",
											   cMaxIncludeDepth, sourceDir.string() ) );

	// #include <module> - a name registered in one of the module directories.
	if ( const auto moduleName = IncludeArgument( line, '<', '>' ); not moduleName.empty() )
	{
		const auto iter = modules.find( string( moduleName ) );
		if ( iter == modules.end() )
			throw std::runtime_error( std::format( "No such module name {}", moduleName ) );

		ProcessedSource spliced = ProcessSource( iter->second.mText, iter->second.mDir,
												 modules, depth + 1 );
		const auto enumValue = magic_enum::enum_cast<ShaderModule>( moduleName, magic_enum::case_insensitive );
		spliced.mModules.set( enumValue.value_or( ShaderModule::None ) );
		return spliced;
	}

	// #include "path" - relative to the including file. Registering a module is
	// the right call for an engine-owned interface and the wrong one for a user
	// splitting their own shader in two.
	if ( const auto relative = IncludeArgument( line, '"', '"' ); not relative.empty() )
	{
		const path included = sourceDir / relative;
		if ( not filesystem::exists( included ) )
			throw std::runtime_error( std::format( "No such included file: {}", included.string() ) );

		return ProcessFile( included, modules, depth + 1 );
	}

	throw std::runtime_error( std::format( "Malformed #include, expected <module> or a quoted path: {}", line ) );
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


ProcessedSource ExpandShaderIncludes( const path& shaderPath, const ShaderModuleTable& modules )
{
	return ProcessFile( shaderPath, modules, 0 );
}

}
