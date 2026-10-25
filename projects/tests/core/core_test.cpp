#include "bubble/core/assert.hpp"
#include "bubble/core/log.hpp"
#include "bubble/core/snake_case.hpp"
#include "bubble/types/thread.hpp"
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

TEST_CASE( "ToSnakeCase keeps an acronym with the word it leads" )
{
    // The last capital of an acronym starts a word only when a lowercase
    // letter follows it; a run of capitals with nothing after stays one word.
    CHECK( ToSnakeCase( "HTTPServer" ) == "http_server" );
    CHECK( ToSnakeCase( "MyID" ) == "my_id" );
    CHECK( ToSnakeCase( "GPUAPI" ) == "gpuapi" );
    CHECK( ToSnakeCase( "ABc" ) == "a_bc" );
}

TEST_CASE( "ToSnakeCase starts a word after a digit as well" )
{
    CHECK( ToSnakeCase( "Lod2D" ) == "lod2_d" );
    CHECK( ToSnakeCase( "9Lives" ) == "9_lives" );
    CHECK( ToSnakeCase( "Vec3" ) == "vec3" );
    // A name already partly snake_case gets no second underscore.
    CHECK( ToSnakeCase( "some_Name" ) == "some_name" );
    CHECK( ToSnakeCase( "_Foo" ) == "_foo" );
    CHECK( ToSnakeCase( "a__B" ) == "a__b" );
    // Snake of a snake is the same snake.
    for ( const string_view name : { "OuterCutOff", "GPUTexture", "ID2Name", "already_snake" } )
        CHECK( ToSnakeCase( ToSnakeCase( name ) ) == ToSnakeCase( name ) );
}

TEST_CASE( "ToSnakeCase leaves what is not ASCII alone" )
{
    // Identifiers are ASCII; only ASCII capitals are lowered, and the other
    // bytes pass through untouched, so the text is never cut in half.
    CHECK( ToSnakeCase( "Caffè" ) == "caffè" );
    CHECK( ToSnakeCase( "Översikt" ) == "Översikt" );
    CHECK( ToSnakeCase( "\xFF\xFE" ) == "\xFF\xFE" );
}

TEST_CASE( "TryParse reads digits and nothing else" )
{
    CHECK( TryParse( "007" ) == 7 );
    CHECK( TryParse( "-0" ) == 0 );
    // Not a whole number: a sign that is not minus, a space, a prefix.
    for ( const string_view bad : { "+42", " 42", "42 ", "0x2a", "1,000", "42\x21" } )
        CHECK_FALSE( TryParse( bad ) );
    // A byte that is not a digit, NUL included, ends the number and so spoils
    // the "whole text" check.
    CHECK_FALSE( TryParse( string_view( "42\0", 3 ) ) );
}

TEST_CASE( "TryParse refuses what does not fit the type" )
{
    CHECK( TryParse( "2147483647" ) == 2147483647 );
    CHECK_FALSE( TryParse( "2147483648" ) );
    CHECK_FALSE( TryParse( "-2147483649" ) );
    CHECK( TryParse<i64>( "-9223372036854775808" ) == -9223372036854775807 - 1 );
    CHECK( TryParse<i8>( "127" ) == 127 );
    CHECK( TryParse<i8>( "-128" ) == -128 );
    CHECK_FALSE( TryParse<i8>( "128" ) );
    // An unsigned type has no minus, so -1 is not a number for it.
    CHECK_FALSE( TryParse<u64>( "-1" ) );
    CHECK( TryParse<u64>( "18446744073709551615" ) == ~u64( 0 ) );
    CHECK( TryParse<u8>( "255" ) == 255 );
    CHECK_FALSE( TryParse<u8>( "256" ) );
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

namespace
{
// The log is one for the whole process, so these tests mute stdout while they
// work and read only what they themselves logged.
struct Muted
{
    Muted() { LogMuteOutput( true ); }
    ~Muted() { LogMuteOutput( false ); }
};
}

TEST_CASE( "The log keeps the newest part of its history and counts what it dropped" )
{
    const Muted muted;
    vector<LogEntry> skipped;
    const u64 from = LogReadSince( 0, skipped );
    for ( int i = 0; i < 21000; ++i )
        LogInfo( "message {}", i );

    vector<LogEntry> entries;
    const u64 next = LogReadSince( from, entries );
    CHECK( next == from + 21000 );
    CHECK( entries.size() < 21000 );
    REQUIRE( not entries.empty() );
    // The oldest messages are gone, but the indexes of what is left run on
    // without a hole, so a reader that fell behind just gets less.
    CHECK( entries.front().mIndex == next - entries.size() );
    CHECK( entries.back().mIndex == next - 1 );
    CHECK( entries.back().mText == "message 20999" );

    // A reader that fell behind the window starts at the oldest thing kept.
    const size_t kept = entries.size();
    entries.clear();
    CHECK( LogReadSince( from, entries ) == next );
    CHECK( entries.size() == kept );
    CHECK( entries.front().mIndex == next - kept );
}

TEST_CASE( "Reading the log from an index it has not reached says nothing new" )
{
    const Muted muted;
    vector<LogEntry> entries{ LogEntry{ LogLevel::Info, "kept by the caller", 0 } };
    const u64 next = LogReadSince( u64( 1 ) << 62, entries );
    CHECK( entries.size() == 1 );
    LogInfo( "after" );
    vector<LogEntry> later;
    CHECK( LogReadSince( next, later ) > next );
    REQUIRE( later.size() == 1 );
    CHECK( later[0].mText == "after" );
    // Asking again with the index it gave back brings nothing.
    CHECK( LogReadSince( next + 1, later ) == next + 1 );
    CHECK( later.size() == 1 );
}

TEST_CASE( "The log takes messages from several threads at once" )
{
    const Muted muted;
    vector<LogEntry> skipped;
    const u64 from = LogReadSince( 0, skipped );
    vector<thread> workers;
    for ( int t = 0; t < 4; ++t )
        workers.push_back( thread( [t] {
            for ( int i = 0; i < 500; ++i )
                LogInfo( "thread {} message {}", t, i );
        } ) );
    for ( thread& worker : workers )
        worker.join();

    vector<LogEntry> entries;
    const u64 next = LogReadSince( from, entries );
    REQUIRE( entries.size() == 2000 );
    CHECK( next == from + 2000 );
    CHECK( entries.front().mIndex == from );
    CHECK( entries.back().mIndex == next - 1 );
    // Nothing was lost and nothing was written twice: a message of one thread
    // is not cut in half by a message of another.
    hset<string> seen;
    for ( const LogEntry& entry : entries )
        seen.insert( entry.mText );
    CHECK( seen.size() == 2000 );
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
