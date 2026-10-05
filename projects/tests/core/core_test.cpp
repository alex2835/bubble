#include "bubble/core/assert.hpp"
#include "bubble/core/log.hpp"
#include "bubble/core/snake_case.hpp"
#include "bubble/types/types.hpp"
#include <doctest.h>

using namespace bubble;

TEST_CASE( "ToSnakeCase splits PascalCase and keeps acronyms whole" )
{
    CHECK( ToSnakeCase( "OuterCutOff" ) == "outer_cut_off" );
    CHECK( ToSnakeCase( "RigidBody" ) == "rigid_body" );
    CHECK( ToSnakeCase( "Light" ) == "light" );
    CHECK( ToSnakeCase( "GPUTexture" ) == "gpu_texture" );
    CHECK( ToSnakeCase( "UseGPU" ) == "use_gpu" );
    CHECK( ToSnakeCase( "Vec3Length" ) == "vec3_length" );
    CHECK( ToSnakeCase( "already_snake" ) == "already_snake" );
    CHECK( ToSnakeCase( "" ).empty() );
}

TEST_CASE( "TryParse takes the whole text or nothing" )
{
    CHECK( TryParse( "42" ) == 42 );
    CHECK( TryParse( "-7" ) == -7 );
    CHECK_FALSE( TryParse( "42x" ) );
    CHECK_FALSE( TryParse( "" ) );
    CHECK( TryParse<u64>( "18446744073709551615" ) == ~u64( 0 ) );
}

TEST_CASE( "A string-keyed hmap and hset find a key by string_view" )
{
    hmap<string, int> map{ { "jump", 1 } };
    hset<string> set{ "run" };
    CHECK( map.find( string_view( "jump" ) ) != map.end() );
    CHECK( set.contains( string_view( "run" ) ) );
    // Other keys keep the plain hash.
    hmap<int, int> numbers{ { 1, 2 } };
    CHECK( numbers.at( 1 ) == 2 );
}

TEST_CASE( "The log keeps what was said, in order, with its level" )
{
    vector<LogEntry> before;
    const u64 from = LogReadSince( 0, before );

    LogInfo( "hello {}", 1 );
    LogWarning( "careful" );
    LogError( "{} + {} = {}", 2, 2, 5 );

    vector<LogEntry> entries;
    const u64 next = LogReadSince( from, entries );
    REQUIRE( entries.size() == 3 );
    CHECK( next == from + 3 );
    CHECK( entries[0].mText == "hello 1" );
    CHECK( entries[0].mLevel == LogLevel::Info );
    CHECK( entries[1].mLevel == LogLevel::Warning );
    CHECK( entries[2].mText == "2 + 2 = 5" );
    CHECK( entries[2].mLevel == LogLevel::Error );
}

TEST_CASE( "An assert that holds lets the code go on" )
{
    int checked = 0;
    BUBBLE_ASSERT( ++checked > 0 or true, "never fails" );
#ifdef NDEBUG
    // Release does not evaluate the condition.
    CHECK( checked == 0 );
#else
    CHECK( checked == 1 );
#endif
}
