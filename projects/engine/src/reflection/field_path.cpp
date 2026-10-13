#include "bubble/reflection/field_path.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
expected<FieldPath, string> FieldPath::TryParse( string_view text )
{
    if ( text.empty() )
        return unexpected( "an empty field path"s );

    FieldPath path;
    size_t pos = 0;
    while ( pos <= text.size() )
    {
        // A name, up to the next . or [.
        const size_t end = text.find_first_of( ".[", pos );
        const string_view segment = text.substr( pos, end == string_view::npos ? string_view::npos : end - pos );
        if ( segment.empty() )
            return unexpected( format( "{}: an empty step", text ) );
        const auto nameMaybe = Name::TryFrom( segment );
        if ( not nameMaybe )
            return unexpected( format( "{}: {}", text, nameMaybe.error() ) );
        path.mSteps.push_back( *nameMaybe );
        pos = end == string_view::npos ? text.size() : end;

        // Positions after it: [2], [2][1].
        while ( pos < text.size() and text[pos] == '[' )
        {
            const size_t close = text.find( ']', pos );
            if ( close == string_view::npos )
                return unexpected( format( "{}: [ without ]", text ) );
            const auto indexMaybe = bubble::TryParse<size_t>( text.substr( pos + 1, close - pos - 1 ) );
            if ( not indexMaybe or *indexMaybe == 0 )
                return unexpected( format( "{}: a position is a whole number from 1, not '{}'", text,
                                           text.substr( pos + 1, close - pos - 1 ) ) );
            path.mSteps.push_back( *indexMaybe );
            pos = close + 1;
        }

        if ( pos == text.size() )
            break;
        if ( text[pos] != '.' )
            return unexpected( format( "{}: '{}' after a position, where . or [ goes", text, text[pos] ) );
        ++pos;
        if ( pos == text.size() )
            return unexpected( format( "{}: an empty step", text ) );
    }
    return path;
}

string FieldPath::Prefix( size_t count ) const
{
    string text;
    for ( size_t i = 0; i < count and i < mSteps.size(); ++i )
    {
        if ( const size_t* position = get_if<size_t>( &mSteps[i] ) )
            text += format( "[{}]", *position );
        else
            text += ( text.empty() ? "" : "." ) + get<Name>( mSteps[i] ).ToString();
    }
    return text;
}
}
