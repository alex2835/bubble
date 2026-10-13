#pragma once
#include "bubble/types/string.hpp"
#include "bubble/types/utility.hpp"

namespace bubble
{
// What a person wrote: names of entities, texts of the UI, translations.
// Valid UTF-8 in NFC, so a word typed with "é" as one code point and as "e"
// with an accent is the same Text, compares equal and hashes alike. Made
// only by From, which checks and normalizes, so every Text is good.
//
// size() of the string counts bytes, right for memory and files. What a
// person counts as characters - the cursor, Backspace, a length limit - are
// graphemes: "é" in two code points is one, so is a thumbs-up with a skin
// tone.
class Text
{
public:
    Text() = default;

    static expected<Text, string> TryFrom( string_view utf8 );

    const string& String() const { return mText; }
    string_view View() const { return mText; }
    bool Empty() const { return mText.empty(); }

    size_t Graphemes() const;
    // The first `count` graphemes; all of it when it has fewer.
    Text Truncated( size_t count ) const;

    friend bool operator==( const Text&, const Text& ) = default;
    friend auto operator<=>( const Text&, const Text& ) = default;

private:
    explicit Text( string text ) : mText( std::move( text ) ) {}

    string mText;
};
}
