#include "bubble/data/json.hpp"
#include "bubble/core/utf8.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
// Recursive descent over the grammar of RFC 8259, keeping the line and
// column it is at, so every value knows where it started and every error
// says where it is.
class JsonParser
{
public:
    explicit JsonParser( string_view text ) : mText( text ) {}

    expected<Json, string> TryDocument()
    {
        if ( not Utf8Valid( mText ) )
            return unexpected( "the text is not valid UTF-8"s );
        auto valueMaybe = TryParseValue( 0 );
        if ( not valueMaybe )
            return valueMaybe;
        SkipSpace();
        if ( not AtEnd() )
            return Fail( format( "the end expected after the value, got {}", Here() ) );
        return valueMaybe;
    }

private:
    // Deeper than any file a person writes; past it, a broken or hostile
    // file would take the stack.
    static constexpr int cMaxDepth = 256;

    expected<Json, string> TryParseValue( int depth )
    {
        if ( depth > cMaxDepth )
            return Fail( format( "nested deeper than {}", cMaxDepth ) );
        SkipSpace();
        Json json;
        json.mLine = mLine;
        json.mColumn = mColumn;
        if ( AtEnd() )
            return Fail( "a value expected, got the end" );

        const char c = Peek();
        if ( c == '{' )
        {
            auto objectMaybe = TryObjectBody( depth );
            if ( not objectMaybe )
                return unexpected( std::move( objectMaybe.error() ) );
            json.mData = std::move( *objectMaybe );
        }
        else if ( c == '[' )
        {
            auto arrayMaybe = TryArrayBody( depth );
            if ( not arrayMaybe )
                return unexpected( std::move( arrayMaybe.error() ) );
            json.mData = std::move( *arrayMaybe );
        }
        else if ( c == '"' )
        {
            auto textMaybe = TryString();
            if ( not textMaybe )
                return unexpected( std::move( textMaybe.error() ) );
            json.mData = std::move( *textMaybe );
        }
        else if ( c == '-' or ( c >= '0' and c <= '9' ) )
        {
            auto numberMaybe = TryNumber();
            if ( not numberMaybe )
                return unexpected( std::move( numberMaybe.error() ) );
            json.mData = *numberMaybe;
        }
        else if ( Word( "true" ) )
            json.mData = true;
        else if ( Word( "false" ) )
            json.mData = false;
        else if ( not Word( "null" ) )
            return Fail( format( "a value expected, got {}", Here() ) );
        return json;
    }

    expected<Json::Object, string> TryObjectBody( int depth )
    {
        Advance(); // {
        Json::Object object;
        SkipSpace();
        if ( Take( '}' ) )
            return object;
        for ( ;; )
        {
            SkipSpace();
            if ( AtEnd() or Peek() != '"' )
                return Fail( format( "a key in quotes expected, got {}", Here() ) );
            auto keyMaybe = TryString();
            if ( not keyMaybe )
                return unexpected( std::move( keyMaybe.error() ) );
            SkipSpace();
            if ( not Take( ':' ) )
                return Fail( format( ": expected after the key, got {}", Here() ) );
            auto valueMaybe = TryParseValue( depth + 1 );
            if ( not valueMaybe )
                return unexpected( std::move( valueMaybe.error() ) );
            object.push_back( Json::Member{ std::move( *keyMaybe ), std::move( *valueMaybe ) } );
            SkipSpace();
            if ( Take( '}' ) )
                return object;
            if ( not Take( ',' ) )
                return Fail( format( ", or }} expected, got {}", Here() ) );
        }
    }

    expected<Json::Array, string> TryArrayBody( int depth )
    {
        Advance(); // [
        Json::Array array;
        SkipSpace();
        if ( Take( ']' ) )
            return array;
        for ( ;; )
        {
            auto valueMaybe = TryParseValue( depth + 1 );
            if ( not valueMaybe )
                return unexpected( std::move( valueMaybe.error() ) );
            array.push_back( std::move( *valueMaybe ) );
            SkipSpace();
            if ( Take( ']' ) )
                return array;
            if ( not Take( ',' ) )
                return Fail( format( ", or ] expected, got {}", Here() ) );
        }
    }

    expected<string, string> TryString()
    {
        Advance(); // "
        string text;
        for ( ;; )
        {
            if ( AtEnd() )
                return Fail( "the end of the text inside a string" );
            const char c = Peek();
            if ( c == '"' )
            {
                Advance();
                return text;
            }
            if ( static_cast<unsigned char>( c ) < 0x20 )
                return Fail( "a control character inside a string: write a line break as \\n, a tab as \\t" );
            if ( c != '\\' )
            {
                text += c;
                Advance();
                continue;
            }
            Advance();
            if ( AtEnd() )
                return Fail( "the end of the text inside a string" );
            const char escape = Peek();
            switch ( escape )
            {
                case '"': text += '"'; break;
                case '\\': text += '\\'; break;
                case '/': text += '/'; break;
                case 'b': text += '\b'; break;
                case 'f': text += '\f'; break;
                case 'n': text += '\n'; break;
                case 'r': text += '\r'; break;
                case 't': text += '\t'; break;
                case 'u':
                {
                    auto codePointMaybe = TryUnicode();
                    if ( not codePointMaybe )
                        return unexpected( std::move( codePointMaybe.error() ) );
                    Utf8Append( text, *codePointMaybe );
                    continue;
                }
                default:
                    return Fail(
                        format( "\\{} is not an escape (escapes: \\\" \\\\ \\/ \\b \\f \\n \\r \\t \\u)", escape ) );
            }
            Advance();
        }
    }

    // After the u of \uXXXX; a pair of them for a code point past U+FFFF.
    expected<char32_t, string> TryUnicode()
    {
        auto firstMaybe = TryHex4();
        if ( not firstMaybe )
            return firstMaybe;
        if ( *firstMaybe < 0xD800 or *firstMaybe > 0xDFFF )
            return *firstMaybe;
        if ( *firstMaybe >= 0xDC00 )
            return Fail( "\\u escape: a low surrogate with no high one before it" );
        if ( not( Take( '\\' ) and not AtEnd() and Peek() == 'u' ) )
            return Fail( "\\u escape: a high surrogate with no low one after it" );
        auto secondMaybe = TryHex4();
        if ( not secondMaybe )
            return secondMaybe;
        if ( *secondMaybe < 0xDC00 or *secondMaybe > 0xDFFF )
            return Fail( "\\u escape: a high surrogate with no low one after it" );
        return 0x10000 + ( ( *firstMaybe - 0xD800 ) << 10 ) + ( *secondMaybe - 0xDC00 );
    }

    // The u and four hex digits.
    expected<char32_t, string> TryHex4()
    {
        Advance(); // u
        char32_t value = 0;
        for ( int i = 0; i < 4; ++i )
        {
            const char c = AtEnd() ? '\0' : Peek();
            const int digit = c >= '0' and c <= '9'   ? c - '0'
                              : c >= 'a' and c <= 'f' ? c - 'a' + 10
                              : c >= 'A' and c <= 'F' ? c - 'A' + 10
                                                      : -1;
            if ( digit < 0 )
                return Fail( "\\u escape: four hex digits expected" );
            value = value * 16 + static_cast<char32_t>( digit );
            Advance();
        }
        return value;
    }

    expected<f64, string> TryNumber()
    {
        // JSON's form, checked here: from_chars takes more (inf, a leading
        // +, hex) than a file should hold.
        const size_t start = mPos;
        const auto digits = [&] {
            const size_t from = mPos;
            while ( not AtEnd() and Peek() >= '0' and Peek() <= '9' )
                Advance();
            return mPos - from;
        };
        Take( '-' );
        if ( Take( '0' ) )
        {
            if ( not AtEnd() and Peek() >= '0' and Peek() <= '9' )
                return Fail( "a number does not start with 0 unless it is 0" );
        }
        else if ( digits() == 0 )
            return Fail( format( "a digit expected, got {}", Here() ) );
        if ( Take( '.' ) and digits() == 0 )
            return Fail( format( "a digit expected after the point, got {}", Here() ) );
        if ( Take( 'e' ) or Take( 'E' ) )
        {
            if ( not Take( '+' ) )
                Take( '-' );
            if ( digits() == 0 )
                return Fail( format( "a digit expected in the exponent, got {}", Here() ) );
        }
        const string_view text = mText.substr( start, mPos - start );
        f64 value = 0;
        const auto [end, error] = from_chars( text.data(), text.data() + text.size(), value );
        if ( error != errc() or not isfinite( value ) )
            return Fail( format( "{} is past what a double holds", text ) );
        return value;
    }

    bool Word( string_view word )
    {
        if ( not mText.substr( mPos ).starts_with( word ) )
            return false;
        for ( size_t i = 0; i < word.size(); ++i )
            Advance();
        return true;
    }

    void SkipSpace()
    {
        while ( not AtEnd() and ( Peek() == ' ' or Peek() == '\t' or Peek() == '\n' or Peek() == '\r' ) )
            Advance();
    }

    bool AtEnd() const { return mPos >= mText.size(); }
    char Peek() const { return mText[mPos]; }

    bool Take( char c )
    {
        if ( AtEnd() or Peek() != c )
            return false;
        Advance();
        return true;
    }

    void Advance()
    {
        const auto byte = static_cast<unsigned char>( mText[mPos++] );
        if ( byte == '\n' )
        {
            ++mLine;
            mColumn = 1;
        }
        // A column counts code points: a continuation byte is not a new one.
        else if ( ( byte & 0xC0 ) != 0x80 )
            ++mColumn;
    }

    // What is at the current place, for "expected X, got Y".
    string Here() const
    {
        if ( AtEnd() )
            return "the end";
        size_t end = mPos;
        Utf8Next( mText, end );
        return string( mText.substr( mPos, end - mPos ) );
    }

    unexpected<string> Fail( string_view what ) const
    {
        return unexpected( format( "{}:{}: {}", mLine, mColumn, what ) );
    }

    string_view mText;
    size_t mPos = 0;
    u32 mLine = 1;
    u32 mColumn = 1;
};

namespace
{
string Quoted( string_view text )
{
    string out = "\"";
    for ( const char c : text )
    {
        switch ( c )
        {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if ( static_cast<unsigned char>( c ) < 0x20 )
                    out += format( "\\u{:04x}", c );
                else
                    out += c;
        }
    }
    return out + "\"";
}

string NumberText( f64 number )
{
    if ( not isfinite( number ) )
        return "null";
    // The shortest text that reads back as the same double: 0.1, not
    // 0.10000000000000001; 3, not 3.0.
    char buffer[32];
    const auto [end, error] = to_chars( buffer, buffer + sizeof( buffer ), number );
    return string( buffer, end );
}

bool Scalar( const Json& json )
{
    return not json.Is<Json::Array>() and not json.Is<Json::Object>();
}

bool InlineArray( const Json& json )
{
    const auto arrayMaybe = json.TryAs<Json::Array>();
    return arrayMaybe and ranges::all_of( *arrayMaybe, Scalar );
}

// On one line: an array of scalars - [1, 0.5, 0] - and an object of one
// member that is one or a scalar - { "$vec3": [1, 2, 3] }, { "value": 50 }.
bool Inline( const Json& json )
{
    if ( json.Is<Json::Array>() )
        return InlineArray( json );
    if ( const auto objectMaybe = json.TryAs<Json::Object>() )
        return objectMaybe->size() == 1 and
               ( Scalar( objectMaybe->front().mValue ) or InlineArray( objectMaybe->front().mValue ) );
    return true;
}

void WriteTo( string& out, const Json& json, int indent )
{
    const string pad( static_cast<size_t>( indent + 2 ), ' ' );
    if ( const auto arrayMaybe = json.TryAs<Json::Array>() )
    {
        if ( arrayMaybe->empty() or Inline( json ) )
        {
            out += '[';
            for ( size_t i = 0; i < arrayMaybe->size(); ++i )
            {
                out += i == 0 ? "" : ", ";
                WriteTo( out, ( *arrayMaybe )[i], indent );
            }
            out += ']';
            return;
        }
        out += "[\n";
        for ( size_t i = 0; i < arrayMaybe->size(); ++i )
        {
            out += pad;
            WriteTo( out, ( *arrayMaybe )[i], indent + 2 );
            out += i + 1 < arrayMaybe->size() ? ",\n" : "\n";
        }
        out += string( static_cast<size_t>( indent ), ' ' ) + "]";
    }
    else if ( const auto objectMaybe = json.TryAs<Json::Object>() )
    {
        if ( objectMaybe->empty() )
        {
            out += "{}";
            return;
        }
        if ( Inline( json ) )
        {
            out += "{ " + Quoted( objectMaybe->front().mKey ) + ": ";
            WriteTo( out, objectMaybe->front().mValue, indent );
            out += " }";
            return;
        }
        out += "{\n";
        for ( size_t i = 0; i < objectMaybe->size(); ++i )
        {
            out += pad + Quoted( ( *objectMaybe )[i].mKey ) + ": ";
            WriteTo( out, ( *objectMaybe )[i].mValue, indent + 2 );
            out += i + 1 < objectMaybe->size() ? ",\n" : "\n";
        }
        out += string( static_cast<size_t>( indent ), ' ' ) + "}";
    }
    else if ( const auto textMaybe = json.TryAs<string>() )
        out += Quoted( *textMaybe );
    else if ( const auto numberMaybe = json.TryAs<f64>() )
        out += NumberText( *numberMaybe );
    else if ( const auto flagMaybe = json.TryAs<bool>() )
        out += *flagMaybe ? "true" : "false";
    else
        out += "null";
}
}

Json::Json() = default;

Json::Json( bool value ) : mData( value )
{
}

Json::Json( f64 value ) : mData( value )
{
}

Json::Json( string value ) : mData( std::move( value ) )
{
}

Json::Json( Array value ) : mData( std::move( value ) )
{
}

Json::Json( Object value ) : mData( std::move( value ) )
{
}

Json::Json( const Json& ) = default;
Json::Json( Json&& ) noexcept = default;
Json& Json::operator=( const Json& ) = default;
Json& Json::operator=( Json&& ) noexcept = default;
Json::~Json() = default;

expected<Json, string> Json::TryParse( string_view text )
{
    return JsonParser( text ).TryDocument();
}

string Json::Write() const
{
    string out;
    WriteTo( out, *this, 0 );
    return out + "\n";
}

bool Json::IsNull() const
{
    return holds_alternative<monostate>( mData );
}

OptRef<const Json> Json::TryFind( string_view key ) const
{
    if ( const auto objectMaybe = TryAs<Object>() )
        for ( const Member& member : *objectMaybe )
            if ( member.mKey == key )
                return member.mValue;
    return nullopt;
}

string Json::Where() const
{
    return mLine == 0 ? string() : format( "{}:{}", mLine, mColumn );
}

string Json::Describe() const
{
    if ( const auto arrayMaybe = TryAs<Array>() )
        return format( "an array of {}", arrayMaybe->size() );
    if ( Is<Object>() )
        return "an object";
    if ( const auto textMaybe = TryAs<string>() )
        return Quoted( *textMaybe );
    if ( const auto numberMaybe = TryAs<f64>() )
        return NumberText( *numberMaybe );
    if ( const auto flagMaybe = TryAs<bool>() )
        return *flagMaybe ? "true" : "false";
    return "null";
}
}
