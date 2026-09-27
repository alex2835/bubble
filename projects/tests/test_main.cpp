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

// bubble_tests [part of a name]: only the tests whose names contain it.
int main( int argc, char** argv )
{
    const string_view only = argc > 1 ? argv[1] : "";
    for ( const auto& [name, fn] : test::Tests() )
    {
        if ( not string_view( name ).contains( only ) )
            continue;
        // Flushed, so a test that crashes is the last name printed.
        std::println( "{}", name );
        std::fflush( stdout );
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
