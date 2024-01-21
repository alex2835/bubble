
#include "hot_reloader_export.hpp"


int mul( int a, int b )
{
    return a * b;
}
HR_REGISTER_FUNC( int, mul, int, int )
