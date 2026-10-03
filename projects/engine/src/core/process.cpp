#include "bubble/core/process.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace bubble
{
void InitProcess()
{
#ifdef _WIN32
    SetConsoleOutputCP( CP_UTF8 );
    SetConsoleCP( CP_UTF8 );
#endif
}

bool ProcessUsesUtf8()
{
#ifdef _WIN32
    return GetACP() == CP_UTF8;
#else
    return true;
#endif
}
}
