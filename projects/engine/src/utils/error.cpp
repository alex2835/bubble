
#include <GL/glew.h>
#include "engine/log/log.hpp"
#include "engine/utils/error.hpp"
#include "engine/types/number.hpp"
#include "engine/types/string.hpp"

namespace bubble
{
string_view GLErrorString( i32 errorCode )
{
    switch ( errorCode )
    {
        // OpenGL 2 errors (8)
        case GL_NO_ERROR:
            return "GL_NO_ERROR"sv;
        case GL_INVALID_ENUM:
            return "GL_INVALID_ENUM"sv;
        case GL_INVALID_VALUE:
            return "GL_INVALID_VALUE"sv;
        case GL_INVALID_OPERATION:
            return "GL_INVALID_OPERATION"sv;
        case GL_STACK_OVERFLOW:
            return "GL_STACK_OVERFLOW"sv;
        case GL_STACK_UNDERFLOW:
            return "GL_STACK_UNDERFLOW"sv;
        case GL_OUT_OF_MEMORY:
            return "GL_OUT_OF_MEMORY"sv;
        case GL_TABLE_TOO_LARGE:
            return "GL_TABLE_TOO_LARGE"sv;
        // OpenGL 3 errors (1)
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            return "GL_INVALID_FRAMEBUFFER_OPERATION"sv;
        // GLES 2, 3 and gl 4 error are handled by the switch above
        default:
            return "unknown error"sv;
    }
}


void GLClearError()
{
    int i = 0;
    while ( glGetError() != GL_NO_ERROR and i++ < 100 );
}


void PrintOpenGLErrors( string_view function, string_view file, i32 line )
{
    i32 errorCode = glGetError();
    if ( errorCode != GL_NO_ERROR )
    {
        auto errorString = GLErrorString( errorCode );
        LogError( "OpenGL Error: {} Code: {} \n\tFile: {} Line: {} \n\tFunction: {}", errorString, errorCode, file, line, function );
    }
}

}
