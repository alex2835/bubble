#pragma once 
#include <GL/glew.h>
#include <cstdlib>
#include <iostream>

namespace bubble
{
#define BUBBLE_ASSERT( x, str ) assert( x && str );

inline void GLClearError()
{
    while( glGetError() != GL_NO_ERROR );
}

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
#define glcall(x) GLClearError(); x; bubble::PrintOpenGLErrors(#x, __FILE__, __LINE__);
#else
#define glcall(x) (x)
#endif
