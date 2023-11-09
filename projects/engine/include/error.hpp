#pragma once 
#include <GL/glew.h>

namespace bubble
{

inline void PrintOpenGLErrors( char const* const Function, char const* const File, int const Line )
{
	bool Succeeded = true;
	GLenum Error = glGetError();
	if( Error != GL_NO_ERROR )
	{
		char const* ErrorString = (char const*)glewGetErrorString( Error );
		if( ErrorString )
			std::cerr << ( "OpenGL Error in %s at line %d calling function %s: '%s'", File, Line, Function, ErrorString ) << std::endl;
		else
			std::cerr << ( "OpenGL Error in %s at line %d calling function %s: '%d 0x%X'", File, Line, Function, Error, Error ) << std::endl;
	}
}

}

#ifdef _DEBUG
#define CheckedGLCall(x) while(glGetError()); (x); bubble::PrintOpenGLErrors(#x, __FILE__, __LINE__);
#define CheckedGLResult(x) (x); bubble::PrintOpenGLErrors(#x, __FILE__, __LINE__);
#define CheckExistingErrors(x) bubble::PrintOpenGLErrors(">>BEFORE<< "#x, __FILE__, __LINE__);
#else
#define CheckedGLCall(x) (x)
#define CheckExistingErrors(x)
#endif
