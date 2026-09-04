#include "engine/pch/pch.hpp"
#include "engine/loader/loader.hpp"
#include "engine/types/string.hpp"
#include "engine/loader/shader_loader_error_handling.hpp"
#include "engine/loader/shader_module_loader.hpp"
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


ParsedShaders ParseMultipleFiles( path filePath )
{
	ParsedShaders parsedShaders;
	const ShaderModuleTable modules = GetShaderModules();

	filePath.replace_extension( ".vert" );
	if ( filesystem::exists( filePath ) )
	{
		ProcessedSource processed = ExpandShaderIncludes( filePath, modules );
		parsedShaders.vertex = std::move( processed.mText );
		parsedShaders.modules |= processed.mModules;
	}

    filePath.replace_extension( ".geom" );
    if ( filesystem::exists( filePath ) )
    {
        ProcessedSource processed = ExpandShaderIncludes( filePath, modules );
        parsedShaders.geometry = std::move( processed.mText );
		parsedShaders.modules |= processed.mModules;
    }

    filePath.replace_extension( ".frag" );
    if ( filesystem::exists( filePath ) )
    {
        ProcessedSource processed = ExpandShaderIncludes( filePath, modules );
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
			ProcessedSource processed = ExpandShaderIncludes( defaultVertex, modules );
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
	// With a null length array GL reads the pointer as a null-terminated string.
	// It happens to be one today only because these views come from the strings
	// in ParsedShaders; hand this a genuine substring view and it runs off the
	// end.
	const i32 vertex_length = (i32)vertex_source.size();
	glcall( glShaderSource( vertex_shader, 1, &cvertex_source, &vertex_length ) );
	glcall( glCompileShader( vertex_shader ) );
	{
		i32 success;
		glGetShaderiv( vertex_shader, GL_COMPILE_STATUS, &success );
		if ( success != GL_TRUE )
		{
			const string log = ShaderInfoLog( vertex_shader );
			glDeleteShader( vertex_shader );

			ThrowShaderError( shader, "the vertex stage failed to compile",
							  StagePath( shader, ".vert" ), log,
							  StageSources{ .mActive = vertex_source,
											.mVertex = vertex_source,
											.mFragment = fragment_source,
											.mGeometry = geometry_source } );
		}
	}

	// Fragment shader
	i32 fragment_shader = glCreateShader( GL_FRAGMENT_SHADER );
	const char* cfragment_source = fragment_source.data();
	const i32 fragment_length = (i32)fragment_source.size();
	glcall( glShaderSource( fragment_shader, 1, &cfragment_source, &fragment_length ) );
	glcall( glCompileShader( fragment_shader ) );
	{
		i32 success;
		glGetShaderiv( fragment_shader, GL_COMPILE_STATUS, &success );
		if ( success != GL_TRUE )
		{
			const string log = ShaderInfoLog( fragment_shader );
			glDeleteShader( vertex_shader );
			glDeleteShader( fragment_shader );

			ThrowShaderError( shader, "the fragment stage failed to compile",
							  StagePath( shader, ".frag" ), log,
							  StageSources{ .mActive = fragment_source,
											.mVertex = vertex_source,
											.mFragment = fragment_source,
											.mGeometry = geometry_source } );
		}
	}

	// Geometry shader
	i32 geometry_shader = 0;
	if ( geometry_source.size() )
	{
		geometry_shader = glCreateShader( GL_GEOMETRY_SHADER );
		const char* cgeometry_source = geometry_source.data();
		const i32 geometry_length = (i32)geometry_source.size();
		glcall( glShaderSource( geometry_shader, 1, &cgeometry_source, &geometry_length ) );
		glcall( glCompileShader( geometry_shader ) );
		{
			i32 success;
			glGetShaderiv( geometry_shader, GL_COMPILE_STATUS, &success );
			if ( success != GL_TRUE )
			{
				const string log = ShaderInfoLog( geometry_shader );
				glDeleteShader( vertex_shader );
				glDeleteShader( fragment_shader );
				glDeleteShader( geometry_shader );

				ThrowShaderError( shader, "the geometry stage failed to compile",
								  StagePath( shader, ".geom" ), log,
								  StageSources{ .mActive = geometry_source,
												.mVertex = vertex_source,
												.mFragment = fragment_source,
												.mGeometry = geometry_source } );
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
			const string log = ProgramInfoLog( shader.mShaderId );
			glDeleteShader( vertex_shader );
			glDeleteShader( fragment_shader );
			glDeleteShader( geometry_shader );
			glDeleteProgram( shader.mShaderId );
			shader.mShaderId = 0;

			// No active stage to start with: a link log names the stage it is
			// talking about in a section header, and quoting the wrong stage's
			// source is worse than quoting none.
			ThrowShaderError( shader, "the program failed to link", shader.mPath, log,
							  StageSources{ .mActive = {},
											.mVertex = vertex_source,
											.mFragment = fragment_source,
											.mGeometry = geometry_source } );
		}
	}
    glDeleteShader( vertex_shader );
	glDeleteShader( fragment_shader );
    glDeleteShader( geometry_shader );
}


// How many floats or ints a type occupies in glGetUniform's output. Zero for
// anything with no meaningful default value.
static i32 UniformComponentCount( GLSLDataType type )
{
	switch ( type )
	{
		case GLSLDataType::Float:  case GLSLDataType::Int:
		case GLSLDataType::Bool:                            return 1;
		case GLSLDataType::Float2: case GLSLDataType::Int2: return 2;
		case GLSLDataType::Float3: case GLSLDataType::Int3: return 3;
		case GLSLDataType::Float4: case GLSLDataType::Int4: return 4;
		case GLSLDataType::Mat3:                            return 9;
		case GLSLDataType::Mat4:                            return 16;
		case GLSLDataType::Texture2D:                       return 0;
	}
	return 0;
}


// The value the uniform holds in the program as just linked, which is its GLSL
// initializer when it declared one. Read here, before anything draws: shaders
// are cached and shared, so a second entity picking up the same shader would
// otherwise read back whatever the first one last set.
static UniformDefault ReadUniformDefault( u32 shaderId, const string& name, GLSLDataType type )
{
	UniformDefault uniformDefault;
	if ( UniformComponentCount( type ) == 0 )
		return uniformDefault;

	const i32 location = glGetUniformLocation( shaderId, name.c_str() );
	if ( location == -1 )
		return uniformDefault;

	switch ( type )
	{
		case GLSLDataType::Float:  case GLSLDataType::Float2:
		case GLSLDataType::Float3: case GLSLDataType::Float4:
		case GLSLDataType::Mat3:   case GLSLDataType::Mat4:
			glGetUniformfv( shaderId, location, uniformDefault.mFloats.data() );
			break;
		case GLSLDataType::Int:  case GLSLDataType::Bool:
		case GLSLDataType::Int2: case GLSLDataType::Int3: case GLSLDataType::Int4:
			glGetUniformiv( shaderId, location, uniformDefault.mInts.data() );
			break;
		default:
			break;
	}
	return uniformDefault;
}


// Both halves of what introspection finds, returned together rather than one
// returned and the other written into the shader through a reference.
struct ShaderUniforms
{
	UniformDescription mDescriptors;
	UniformDefaults mDefaults;
};

ShaderUniforms LoadShaderUniforms( u32 shaderId )
{
	ShaderUniforms uniforms;

    // Introspect active uniforms - skip engine-managed ones set by the renderer
    // or by BasicMaterial::Apply, which would otherwise show up as per-entity
    // fields that are overwritten on every draw.
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
	glGetProgramiv( shaderId, GL_ACTIVE_UNIFORMS, &numUniforms );
	for ( GLint i = 0; i < numUniforms; i++ )
	{
		char nameBuf[256];
		GLsizei length = 0;
		GLint size = 0;
		GLenum glType = 0;
		glGetActiveUniform( shaderId, i, sizeof( nameBuf ), &length, &size, &glType, nameBuf );

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

		uniforms.mDefaults.emplace( name, ReadUniformDefault( shaderId, name, typeIt->second ) );
		uniforms.mDescriptors.emplace( std::move( name ), typeIt->second );
	}
	return uniforms;
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
	auto [descriptors, defaults] = LoadShaderUniforms( shader->mShaderId );
	shader->mUniformDescriptors = std::move( descriptors );
	shader->mUniformDefaults = std::move( defaults );
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
