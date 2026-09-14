#include "test.hpp"

namespace test
{
namespace
{
int gFailures = 0;

// A function-local static: the registrars run during static initialization,
// in whatever order the linker chose, and this is ready for the first one.
auto& Tests()
{
    static vector<std::pair<const char*, void ( * )()>> tests;
    return tests;
}
}

void Fail( const char* file, int line, const char* expr )
{
    std::println( "  FAIL {}:{}: {}", file, line, expr );
    gFailures++;
}

void Register( const char* name, void ( *fn )() )
{
    Tests().emplace_back( name, fn );
}
}

int main()
{
    for ( const auto& [name, fn] : test::Tests() )
    {
        std::println( "{}", name );
        fn();
    }

    if ( test::gFailures )
    {
        std::println( "{} check(s) failed", test::gFailures );
        return 1;
    }
    std::println( "all passed ({} tests)", test::Tests().size() );
    return 0;
}
