
#include "engine/loader/loader.hpp"
#include <sstream>
#include <fstream>

namespace bubble
{

Ref<Shader> Loader::LoadShader( const std::path& path )
{
	Ref<Shader> shader = CreateRef<Shader>();
	std::string vertexSource, fragmentSource, geometry;

	shader->mName = path.filename().string();
	ParseShaders( path, vertexSource, fragmentSource, geometry );
	CompileShaders( *shader, vertexSource, fragmentSource, geometry );
	return shader;
}


Ref<Shader> Loader::LoadShader( const std::string_view name,
                                const std::string_view vertex,
                                const std::string_view fragment,
                                const std::string_view geometry )
{
	Ref<Shader> shader = CreateRef<Shader>();
	shader->mName = name;
	CompileShaders( *shader, vertex, fragment, geometry );
	mShaders.emplace( name, shader );
	return shader;
}


void Loader::ParseShaders( const std::path& path,
						   std::string& vertex,
						   std::string& fragment,
						   std::string& geometry )
{
	enum ShaderType
	{
		NONE = -1, VERTEX = 0, FRAGMENT = 1, GEOMETRY = 2
	};
	ShaderType type = NONE;

	std::ifstream stream( path );
	if ( !stream.is_open() )
		throw std::runtime_error( std::format( "Incorrect shader file path: {}", path.string() ) );

	std::string line;
	std::stringstream shaders[3];

	while ( std::getline( stream, line ) )
	{
		if ( line.find( "#shader" ) != std::string::npos )
		{
			if ( line.find( "vertex" ) != std::string::npos )
				type = VERTEX;
			else if ( line.find( "fragment" ) != std::string::npos )
				type = FRAGMENT;
			else if ( line.find( "geometry" ) != std::string::npos )
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
							 const std::string_view vertex_source,
							 const std::string_view fragment_source,
                             const std::string_view geometry_source )
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

			std::string log;
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
			std::string log;

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
				std::string log;

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
	shader.mShaderID = glCreateProgram();
	// Link shaders
	glcall( glAttachShader( shader.mShaderID, vertex_shader ) );
	glcall( glAttachShader( shader.mShaderID, fragment_shader ) );
	if ( geometry_source.size() )
	{
		glcall( glAttachShader( shader.mShaderID, geometry_shader ) );
	}
	glcall( glLinkProgram( shader.mShaderID ) );

	{
		GLint success;
		glGetProgramiv( shader.mShaderID, GL_LINK_STATUS, &success );
		if ( success != GL_TRUE )
		{
			GLint max_length = 0;
			std::string log;

			glGetShaderiv( shader.mShaderID, GL_INFO_LOG_LENGTH, &max_length );
			log.resize( max_length );
			glGetProgramInfoLog( shader.mShaderID, max_length, NULL, (GLchar*)log.data() );

			// free resources
			//glDeleteShader(geometry_shader);
			glDeleteShader( vertex_shader );
			glDeleteShader( fragment_shader );
			glDeleteProgram( shader.mShaderID );

			LogError( "LINKING SHADER ERROR: {} \n {}", shader.mName, log );
			throw std::runtime_error( "Shader compilation failed" );
		}
	}
	//glDeleteShader(geometry_shader);
	glDeleteShader( vertex_shader );
	glDeleteShader( fragment_shader );
}

}
