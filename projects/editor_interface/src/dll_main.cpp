
#include "hot_reloader_export.hpp"

int mul( int a, int b )
{
    return a * b + 1;
}
HR_REGISTER_FUNC( int, mul, int, int )


void plus( std::string& out, const std::string& a, const std::string& b )
{
    out = a + b;
}
HR_REGISTER_FUNC( void, plus, std::string&, const std::string&, const std::string& )

