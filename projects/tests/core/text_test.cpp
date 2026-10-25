// Text: what a person wrote, valid UTF-8 in NFC, counted in graphemes.
#include "bubble/core/text.hpp"
#include "bubble/types/stream.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
Text Make( string_view utf8 )
{
    auto textMaybe = Text::TryFrom( utf8 );
    REQUIRE( textMaybe );
    return *textMaybe;
}
}

TEST_CASE( "Text refuses what is not UTF-8" )
{
    CHECK( Text::TryFrom( "\xC3\x28" ).error() == "the text is not valid UTF-8" );
    CHECK( Text::TryFrom( "" ) );
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

TEST_CASE( "Text composes what Unicode composes and leaves the rest apart" )
{
    // NFC joins an accent to its letter, but it does not fold look-alikes:
    // a ligature stays one character of its own, and an accent with no
    // composed form stays a code point of its own.
    const Text ligature = Make( "ﬁle" );
    CHECK( ligature.Graphemes() == 3 );
    CHECK( ligature != Make( "file" ) );
    const Text qdot = Make( "q\u0307" );
    CHECK( qdot.Graphemes() == 1 );
    CHECK( qdot.String().size() == 3 );
    CHECK( qdot.Truncated( 1 ) == qdot );
    // A sign that Unicode maps onto a letter joins with the letter beside it.
    CHECK( Make( "\u212BB" ) == Make( "\u00C5B" ) );
    // Normalizing made the text shorter, so what a person counts is what cuts.
    const Text accent = Make( "e\u0301" );
    CHECK( accent.String().size() == 2 );
    CHECK( accent.Graphemes() == 1 );
    CHECK( accent.Truncated( 1 ) == accent );
    CHECK( accent.Truncated( 0 ).Empty() );
    // An accent with no letter before it is a grapheme of its own.
    CHECK( Make( "\u0301x" ).Graphemes() == 2 );
    CHECK( Make( "\u0301x" ).Truncated( 1 ).View() == "\u0301" );
}

TEST_CASE( "Text takes what a file may hold, not only what a keyboard types" )
{
    const Text withNul = Make( string_view( "a\0b", 3 ) );
    CHECK( withNul.Graphemes() == 3 );
    CHECK( withNul.String().size() == 3 );
    CHECK( withNul.Truncated( 2 ) == Make( string_view( "a\0", 2 ) ) );
    // A byte-order mark is a character like any other: nothing is stripped
    // from either end.
    const string bom = "\xEF\xBB\xBF";
    CHECK( Make( bom + "door" ).Graphemes() == 5 );
    CHECK( Make( "door" + bom ).Graphemes() == 5 );
    CHECK( Make( bom ).Graphemes() == 1 );
    CHECK( Make( bom ).String().size() == 3 );
}

TEST_CASE( "Text orders by code points, not by a person's alphabet" )
{
    // Sorting Text sorts bytes: a capital comes before a lowercase letter and
    // a Russian letter after both. A list the editor shows sorted needs a
    // collation the engine does not do yet.
    CHECK( Make( "B" ) < Make( "a" ) );
    CHECK( Make( "a" ) < Make( "я" ) );
    CHECK( not( Make( "cafe\u0301" ) < Make( "caf\u00E9" ) ) );
    CHECK( Make( "cafe\u0301" ) == Make( "caf\u00E9" ) );
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
