#pragma once 
#include <GL/glew.h>
#include "engine/log/log.hpp"
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
            LogError( "OpenGL Error in {} at line {} calling function {}: '{}'", File, Line, Function, ErrorString );
        else
            LogError( "OpenGL Error in {} at line {} calling function {}: '{}", File, Line, Function, Error );
    }
}

}

#ifdef _DEBUG
#define glcall(x) GLClearError(); x; bubble::PrintOpenGLErrors(#x, __FILE__, __LINE__);
#else
#define glcall(x) (x)
#endif
