// Text: what a person wrote, valid UTF-8 in NFC, counted in graphemes.
#include "bubble/core/text.hpp"
#include "bubble/types/stream.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
Text Make( string_view utf8 )
{
    auto text = Text::From( utf8 );
    REQUIRE( text );
    return *text;
}
}

TEST_CASE( "Text refuses what is not UTF-8" )
{
    CHECK( Text::From( "\xC3\x28" ).error() == "the text is not valid UTF-8" );
    CHECK( Text::From( "" ) );
    CHECK( Make( "дверь" ).View() == "дверь" );
}

TEST_CASE( "The same word typed two ways is the same Text" )
{
    // "é" as one code point, and as "e" with a combining acute accent.
    const Text composed = Make( "café" );
    const Text decomposed = Make( "café" );
    CHECK( composed == decomposed );
    CHECK( decomposed.View() == "café" );
}

TEST_CASE( "Text counts and cuts what a person sees as characters" )
{
    CHECK( Make( "" ).Graphemes() == 0 );
    CHECK( Make( "door" ).Graphemes() == 4 );
    CHECK( Make( "дверь" ).Graphemes() == 5 );
    // A thumbs-up with a skin tone, and a family joined by zero-width joiners.
    const Text emoji = Make( "a\U0001F44D\U0001F3FDb\U0001F468‍\U0001F469‍\U0001F467" );
    CHECK( emoji.Graphemes() == 4 );
    CHECK( emoji.Truncated( 2 ).View() == "a\U0001F44D\U0001F3FD" );
    CHECK( emoji.Truncated( 3 ).View() == "a\U0001F44D\U0001F3FDb" );
    CHECK( emoji.Truncated( 10 ) == emoji );
    CHECK( emoji.Truncated( 0 ).Empty() );
    CHECK( Make( "" ).Truncated( 3 ).Empty() );
    // An accent that was not composed with its letter stays with it.
    CHECK( Make( "q̇x" ).Truncated( 1 ).View() == "q̇" );
}
