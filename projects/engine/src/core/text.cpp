#include "bubble/core/text.hpp"
#include "bubble/core/utf8.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/number.hpp"
#include <utf8proc.h>

namespace bubble
{
namespace
{
bool Ascii( string_view text )
{
    for ( const char c : text )
        if ( static_cast<u8>( c ) >= 0x80 )
            return false;
    return true;
}

// Calls onBreak( byte ) where each grapheme after the first begins, until it
// returns false.
template <typename F>
void ForEachBreak( string_view text, F&& onBreak )
{
    if ( text.empty() )
        return;
    utf8proc_int32_t state = 0;
    size_t pos = 0;
    char32_t previous = Utf8Next( text, pos );
    while ( pos < text.size() )
    {
        const size_t start = pos;
        const char32_t current = Utf8Next( text, pos );
        const bool split = utf8proc_grapheme_break_stateful( static_cast<utf8proc_int32_t>( previous ),
                                                             static_cast<utf8proc_int32_t>( current ), &state );
        if ( split and not onBreak( start ) )
            return;
        previous = current;
    }
}
}

expected<Text, string> Text::TryFrom( string_view utf8 )
{
    if ( not Utf8Valid( utf8 ) )
        return unexpected( "the text is not valid UTF-8"s );
    // ASCII is in NFC already.
    if ( Ascii( utf8 ) )
        return Text( string( utf8 ) );

    utf8proc_uint8_t* normalized = nullptr;
    const utf8proc_ssize_t length = utf8proc_map(
        reinterpret_cast<const utf8proc_uint8_t*>( utf8.data() ), static_cast<utf8proc_ssize_t>( utf8.size() ),
        &normalized, static_cast<utf8proc_option_t>( UTF8PROC_STABLE | UTF8PROC_COMPOSE ) );
    if ( length < 0 )
        return unexpected( format( "the text cannot be normalized: {}", utf8proc_errmsg( length ) ) );
    string text( reinterpret_cast<const char*>( normalized ), static_cast<size_t>( length ) );
    utf8proc_free( normalized );
    return Text( std::move( text ) );
}

size_t Text::Graphemes() const
{
    if ( mText.empty() )
        return 0;
    size_t count = 1;
    ForEachBreak( mText, [&]( size_t ) {
        ++count;
        return true;
    } );
    return count;
}

Text Text::Truncated( size_t count ) const
{
    if ( count == 0 )
        return Text();
    size_t end = mText.size();
    size_t kept = 1;
    ForEachBreak( mText, [&]( size_t start ) {
        if ( kept < count )
        {
            ++kept;
            return true;
        }
        end = start;
        return false;
    } );
    // A cut at a grapheme boundary keeps the text in NFC.
    return Text( mText.substr( 0, end ) );
}
}
