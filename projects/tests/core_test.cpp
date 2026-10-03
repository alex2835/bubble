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

TEST_CASE( "str_hash_map finds a string key by string_view" )
{
    str_hash_map<int> map{ { "jump", 1 } };
    const string_view key = "jump";
    CHECK( map.find( key ) != map.end() );
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
