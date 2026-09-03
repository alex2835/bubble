#include "engine/pch/pch.hpp"
#include "engine/loader/loader.hpp"
#include "engine/types/string.hpp"
#include <GL/glew.h>

namespace bubble
{
enum ShaderType
{
    NONE = -1, 
	VERTEX = 0, 
	FRAGMENT = 1,
	GEOMETRY = 2
};

struct ParsedShaders
{
    string vertex;
	string fragment;
	string geometry;
	ShaderModules modules;
};


// A module's text plus the directory it was found in, so a relative #include
// inside a module resolves against the module and not against whoever
// included it.
struct ModuleSource
{
	string mText;
	path mDir;
};
using ModuleTable = map<string, ModuleSource>;

// One splice of source, and which engine modules it pulled in. Returned by
// value all the way up the recursion rather than accumulated into a stream and
// a bitset threaded down by reference.
struct ProcessedSource
{
	string mText;
	ShaderModules mModules;
};

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


// Read fresh on every shader load. This used to be cached in a function-local
// static, which meant a module edited on disk never took effect - not on a hot
// reload, not on reopening the project, only on restarting the editor.
ModuleTable GetModulesList()
{
	ModuleTable modules;
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
							 ModuleSource{ .mText = filesystem::readFile( file ),
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
									  const ModuleTable& modules,
									  int depth );

static ProcessedSource ProcessFile( const path& shaderPath,
									const ModuleTable& modules,
									int depth );


// One line: itself, or whatever it includes.
static ProcessedSource ProcessLine( string_view line,
									const path& sourceDir,
									const ModuleTable& modules,
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
									  const ModuleTable& modules,
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
									const ModuleTable& modules,
									int depth )
{
	return ProcessSource( filesystem::readFile( shaderPath ), shaderPath.parent_path(), modules, depth );
}


ParsedShaders ParseMultipleFiles( path filePath )
{
	ParsedShaders parsedShaders;
	const ModuleTable modules = GetModulesList();

	filePath.replace_extension( ".vert" );
	if ( filesystem::exists( filePath ) )
	{
		ProcessedSource processed = ProcessFile( filePath, modules, 0 );
		parsedShaders.vertex = std::move( processed.mText );
		parsedShaders.modules |= processed.mModules;
	}

    filePath.replace_extension( ".geom" );
    if ( filesystem::exists( filePath ) )
    {
        ProcessedSource processed = ProcessFile( filePath, modules, 0 );
        parsedShaders.geometry = std::move( processed.mText );
		parsedShaders.modules |= processed.mModules;
    }

    filePath.replace_extension( ".frag" );
    if ( filesystem::exists( filePath ) )
    {
        ProcessedSource processed = ProcessFile( filePath, modules, 0 );
        parsedShaders.fragment = std::move( processed.mText );
		parsedShaders.modules |= processed.mModules;
    }

	// A shader with a fragment stage and no vertex stage of its own gets the
	// engine's. Nearly every shader wants the same vertex stage, and requiring
	// a copy of it per shader is why two shaders in a fresh project end up
	// byte-identical to phong.vert and to each other.
	if ( parsedShaders.vertex.empty() and not parsedShaders.fragment.empty() )
	{
		const path defaultVertex = DEFAULT_VERTEX_SHADER;
		if ( filesystem::exists( defaultVertex ) )
		{
			ProcessedSource processed = ProcessFile( defaultVertex, modules, 0 );
			parsedShaders.vertex = std::move( processed.mText );
			parsedShaders.modules |= processed.mModules;
		}
	}
	return parsedShaders;
}


std::optional<ParsedShaders> ParseShaders( const path& path )
{
	auto parsedShaders = ParseMultipleFiles( path );
	if ( parsedShaders.vertex.empty() || parsedShaders.fragment.empty() )
		return std::nullopt;
	return parsedShaders;
}


void CompileShaders( Shader& shader,
					 string_view vertex_source,
					 string_view fragment_source,
					 string_view geometry_source )
{
	// Vertex shaders
	i32 vertex_shader = glCreateShader( GL_VERTEX_SHADER );
	const char* cvertex_source = vertex_source.data();
	glcall( glShaderSource( vertex_shader, 1, &cvertex_source, NULL ) );
	glcall( glCompileShader( vertex_shader ) );
	{
		i32 success;
		glGetShaderiv( vertex_shader, GL_COMPILE_STATUS, &success );
		if ( success != GL_TRUE )
		{
			i32 max_length = 0;
			glGetShaderiv( vertex_shader, GL_INFO_LOG_LENGTH, &max_length );

			string log;
			log.resize( max_length );
			glGetShaderInfoLog( vertex_shader, max_length, &max_length, (GLchar*)log.data() );
			glDeleteShader( vertex_shader );

			// free resources
			glDeleteShader( vertex_shader );

			LogError( "VERTEX SHADER ERROR: {} \n {}", shader.mName, log );
			throw std::runtime_error( "Shader compilation failed" );
		}
	}

	// Fragment shader
	i32 fragment_shader = glCreateShader( GL_FRAGMENT_SHADER );
	const char* cfragment_source = fragment_source.data();
	glcall( glShaderSource( fragment_shader, 1, &cfragment_source, NULL ) );
	glcall( glCompileShader( fragment_shader ) );
	{
		i32 success;
		glGetShaderiv( fragment_shader, GL_COMPILE_STATUS, &success );
		if ( success != GL_TRUE )
		{
			i32 max_length = 0;
			string log;

			glGetShaderiv( fragment_shader, GL_INFO_LOG_LENGTH, &max_length );
			log.resize( max_length );
			glGetShaderInfoLog( fragment_shader, max_length, &max_length, (GLchar*)log.data() );
			glDeleteShader( fragment_shader );
	
			// free resources
			glDeleteShader( vertex_shader );
			glDeleteShader( fragment_shader );

			LogError( "FRAGMENT SHADER ERROR: : {} \n {}", shader.mName, log );
			throw std::runtime_error( "Shader compilation failed" );
		}
	}

	// Geometry shader
	i32 geometry_shader = 0;
	if ( geometry_source.size() )
	{
		geometry_shader = glCreateShader( GL_GEOMETRY_SHADER );
		const char* cgeometry_source = geometry_source.data();
		glcall( glShaderSource( geometry_shader, 1, &cgeometry_source, NULL ) );
		glcall( glCompileShader( geometry_shader ) );
		{
			i32 success;
			glGetShaderiv( geometry_shader, GL_COMPILE_STATUS, &success );
			if ( success != GL_TRUE )
			{
				i32 max_length = 0;
				string log;

				glGetShaderiv( geometry_shader, GL_INFO_LOG_LENGTH, &max_length );
				log.resize( max_length );
				glGetShaderInfoLog( geometry_shader, max_length, &max_length, (GLchar*)log.data() );

				// free resources
				glDeleteShader( geometry_shader );
				glDeleteShader( vertex_shader );
				glDeleteShader( fragment_shader );

				LogError( "GEOMETRY SHADER ERROR: {} \n {}", shader.mName, log );
				throw std::runtime_error( "Shader compilation failed" );
			}
		}
	}

	// Shader program
	shader.mShaderId = glCreateProgram();
	
	// Link shaders
	glcall( glAttachShader( shader.mShaderId, vertex_shader ) );
	glcall( glAttachShader( shader.mShaderId, fragment_shader ) );
	if ( geometry_source.size() )
		glcall( glAttachShader( shader.mShaderId, geometry_shader ) );
	
	glcall( glLinkProgram( shader.mShaderId ) );
	{
		i32 success;
		glGetProgramiv( shader.mShaderId, GL_LINK_STATUS, &success );
		if ( success != GL_TRUE )
		{
			i32 max_length = 0;
			string log;

			glGetShaderiv( shader.mShaderId, GL_INFO_LOG_LENGTH, &max_length );
			log.resize( max_length );
			glGetProgramInfoLog( shader.mShaderId, max_length, NULL, (GLchar*)log.data() );

			// free resources
			glDeleteShader( vertex_shader );
			glDeleteShader( fragment_shader );
			glDeleteShader( geometry_shader );
			glDeleteProgram( shader.mShaderId );

			LogError( "LINKING SHADER ERROR: {} \n {}", shader.mName, log );
			throw std::runtime_error( "Shader compilation failed" );
		}
	}
    glDeleteShader( vertex_shader );
	glDeleteShader( fragment_shader );
    glDeleteShader( geometry_shader );
}


UniformDescription LoadUniformDescriptions( const Ref<Shader>& shader )
{
	UniformDescription uniformDescriptors;

    // Introspect active uniforms — skip engine-managed ones set by the renderer
    static const std::unordered_set<string_view> sEngineUniforms = { "uModel"sv,  "uLights"sv,
    "uView"sv, "uViewPos"sv, "uProjection"sv, "uNormalMapping"sv, "uNormalMappingStrength"sv,
    "uNumLights"sv, "uNormalMatrix"sv, "uMaterial"sv, };

    static const std::unordered_map<GLenum, GLSLDataType> sGLenumToGLSLType = {
		{ GL_SAMPLER_2D, GLSLDataType::Texture2D },
		{ GL_FLOAT,      GLSLDataType::Float  },
		{ GL_FLOAT_VEC2, GLSLDataType::Float2 },
		{ GL_FLOAT_VEC3, GLSLDataType::Float3 },
		{ GL_FLOAT_VEC4, GLSLDataType::Float4 },
		{ GL_FLOAT_MAT3, GLSLDataType::Mat3   },
		{ GL_FLOAT_MAT4, GLSLDataType::Mat4   },
		{ GL_INT,        GLSLDataType::Int    },
		{ GL_INT_VEC2,   GLSLDataType::Int2   },
		{ GL_INT_VEC3,   GLSLDataType::Int3   },
		{ GL_INT_VEC4,   GLSLDataType::Int4   },
		{ GL_BOOL,       GLSLDataType::Bool   },
	};

	GLint numUniforms = 0;
	glGetProgramiv( shader->mShaderId, GL_ACTIVE_UNIFORMS, &numUniforms );
	for ( GLint i = 0; i < numUniforms; i++ )
	{
		char nameBuf[256];
		GLsizei length = 0;
		GLint size = 0;
		GLenum glType = 0;
		glGetActiveUniform( shader->mShaderId, i, sizeof( nameBuf ), &length, &size, &glType, nameBuf );

		string name( nameBuf, length );
		auto typeIt = sGLenumToGLSLType.find( glType );
		if ( typeIt == sGLenumToGLSLType.end() )
			continue; // skip unsupported types
		
		auto isSystemUni = false;
		for ( const auto& systemUni : sEngineUniforms ) 
			if ( name.starts_with( systemUni ) )
				isSystemUni |= true;
		if ( isSystemUni )
			continue;// skip system uniforms

		uniformDescriptors.emplace( std::move( name ), typeIt->second );
	}
	return uniformDescriptors;
}


Ref<Shader> LoadShader( const path& path )
{
    Ref<Shader> shader = CreateRef<Shader>();
    shader->mName = path.stem().string();
    shader->mPath = path;
    
	auto shaders = ParseShaders( path );
	if ( not shaders )
		return nullptr;

	auto [vertex, fragment, geometry, modules] = *shaders;
    CompileShaders( *shader, vertex, fragment, geometry );
    shader->mModules = std::move( modules );
	shader->mUniformDescriptors = LoadUniformDescriptions( shader );
    return shader;
}


Ref<Shader> Loader::LoadShader( path shaderPath )
{
	auto [relPath, absPath] = RelAbsFromProjectPath( shaderPath.replace_extension() );

    auto iter = mShaders.find( relPath );
    if ( iter != mShaders.end() )
        return iter->second;

    auto shader = bubble::LoadShader( absPath );
	if ( not shader )
	{
		LogError( "Failed to parse shaders: {}", absPath.string() );
		return nullptr;
	}

    mShaders.emplace( relPath, shader );
    mResourcesGeneration = NextResourcesGeneration();
    return shader;
}

}
