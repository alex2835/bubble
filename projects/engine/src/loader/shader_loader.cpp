
#include "engine/loader/loader.hpp"
#include <sstream>
#include <fstream>

namespace bubble
{
Ref<Shader> Loader::JustLoadShader( string name,
                                    string_view vertex,
                                    string_view fragment,
                                    string_view geometry )
{
    Ref<Shader> shader = CreateRef<Shader>();
    shader->mName = std::move( name );
    CompileShaders( *shader, vertex, fragment, geometry );
    return shader;
}

Ref<Shader> Loader::JustLoadShader( const path& path )
{
    Ref<Shader> shader = CreateRef<Shader>();
    shader->mName = path.filename().string();
    string vertexSource, fragmentSource, geometry;
    ParseShaders( path, vertexSource, fragmentSource, geometry );
    CompileShaders( *shader, vertexSource, fragmentSource, geometry );
	return shader;
}

Ref<Shader> Loader::LoadShader( const path& path )
{
    auto iter = mShaders.find( path );
    if ( iter != mShaders.end() )
        return iter->second;

	auto shader = JustLoadShader( path );
    mShaders.emplace( path, shader );
	return shader;
}

void Loader::ParseShaders( const path& path,
						   string& vertex,
						   string& fragment,
						   string& geometry )
{
	enum ShaderType{ NONE = -1, VERTEX = 0, FRAGMENT = 1, GEOMETRY = 2 };
	ShaderType type = NONE;

	std::ifstream stream( path );
	if ( !stream.is_open() )
		throw std::runtime_error( std::format( "Incorrect shader file path: {}", path.string() ) );

	string line;
	std::stringstream shaders[3];

	while ( std::getline( stream, line ) )
	{
		if ( line.find( "#shader" ) != string::npos )
		{
			if ( line.find( "vertex" ) != string::npos )
				type = VERTEX;
			else if ( line.find( "fragment" ) != string::npos )
				type = FRAGMENT;
			else if ( line.find( "geometry" ) != string::npos )
				type = GEOMETRY;
		}
		else if ( type == NONE )
			continue;
		else
			shaders[type] << line << '\n';
	}
	vertex = shaders[VERTEX].str();
	fragment = shaders[FRAGMENT].str();
	geometry = shaders[GEOMETRY].str();
}


void Loader::CompileShaders( Shader& shader,
							 string_view vertex_source,
							 string_view fragment_source,
                             string_view geometry_source )
{
	// Vertex shaders
	GLint vertex_shader = glCreateShader( GL_VERTEX_SHADER );
	const char* cvertex_source = vertex_source.data();
	glcall( glShaderSource( vertex_shader, 1, &cvertex_source, NULL ) );
	glcall( glCompileShader( vertex_shader ) );
	{
		GLint success;
		glGetShaderiv( vertex_shader, GL_COMPILE_STATUS, &success );
		if ( success != GL_TRUE )
		{
			GLint max_length = 0;
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
	GLint fragment_shader = glCreateShader( GL_FRAGMENT_SHADER );
	const char* cfragment_source = fragment_source.data();
	glcall( glShaderSource( fragment_shader, 1, &cfragment_source, NULL ) );
	glcall( glCompileShader( fragment_shader ) );
	{
		GLint success;
		glGetShaderiv( fragment_shader, GL_COMPILE_STATUS, &success );
		if ( success != GL_TRUE )
		{
			GLint max_length = 0;
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
	GLint geometry_shader = 0;
	if ( geometry_source.size() )
	{
		geometry_shader = glCreateShader( GL_GEOMETRY_SHADER );
		const char* cgeometry_source = geometry_source.data();
		glcall( glShaderSource( geometry_shader, 1, &cgeometry_source, NULL ) );
		glcall( glCompileShader( geometry_shader ) );
		{
			GLint success;
			glGetShaderiv( geometry_shader, GL_COMPILE_STATUS, &success );
			if ( success != GL_TRUE )
			{
				GLint max_length = 0;
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
		GLint success;
		glGetProgramiv( shader.mShaderId, GL_LINK_STATUS, &success );
		if ( success != GL_TRUE )
		{
			GLint max_length = 0;
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

}
