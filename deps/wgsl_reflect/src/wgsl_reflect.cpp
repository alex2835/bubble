#include "wgsl_reflect/wgsl_reflect.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>

namespace wgsl_reflect
{
namespace
{

uint32_t RoundUp( uint32_t alignment, uint32_t value )
{
    if ( alignment == 0 )
        return value;
    return ( ( value + alignment - 1 ) / alignment ) * alignment;
}

bool IsIdentStart( char c )
{
    return std::isalpha( (unsigned char)c ) or c == '_';
}

bool IsIdentChar( char c )
{
    return std::isalnum( (unsigned char)c ) or c == '_';
}


// A token stream over WGSL source.
//
// Comments are stripped, but their text is kept against the line they ended so
// a caller can read trailing annotations off a struct member.
struct Token
{
    enum class Kind { End, Ident, Number, Punct } mKind = Kind::End;
    std::string mText;
    uint32_t mLine = 0;
};

class Lexer
{
public:
    explicit Lexer( std::string_view source )
        : mSource( source )
    {
        Tokenize();
    }

    const Token& Peek( size_t lookahead = 0 ) const
    {
        const size_t index = mPos + lookahead;
        return index < mTokens.size() ? mTokens[index] : mEnd;
    }

    Token Next()
    {
        const Token& token = Peek();
        if ( mPos < mTokens.size() )
            mPos++;
        return token;
    }

    bool AtEnd() const { return mPos >= mTokens.size(); }

    bool PeekIs( std::string_view text, size_t lookahead = 0 ) const
    {
        return Peek( lookahead ).mText == text;
    }

    bool Accept( std::string_view text )
    {
        if ( not PeekIs( text ) )
            return false;
        mPos++;
        return true;
    }

    // Comment text that appeared on the given line, without its markers.
    std::string CommentOnLine( uint32_t line ) const
    {
        auto it = std::find_if( mComments.begin(), mComments.end(),
                                [line]( const auto& c ) { return c.first == line; } );
        return it == mComments.end() ? std::string() : it->second;
    }

private:
    void Tokenize()
    {
        uint32_t line = 1;
        size_t i = 0;
        while ( i < mSource.size() )
        {
            const char c = mSource[i];

            if ( c == '\n' )
            {
                line++;
                i++;
                continue;
            }
            if ( std::isspace( (unsigned char)c ) )
            {
                i++;
                continue;
            }

            // Line comment: recorded against its line, then skipped.
            if ( c == '/' and i + 1 < mSource.size() and mSource[i + 1] == '/' )
            {
                const size_t start = i + 2;
                size_t end = start;
                while ( end < mSource.size() and mSource[end] != '\n' )
                    end++;
                std::string text( mSource.substr( start, end - start ) );
                while ( not text.empty() and std::isspace( (unsigned char)text.front() ) )
                    text.erase( text.begin() );
                mComments.emplace_back( line, std::move( text ) );
                i = end;
                continue;
            }

            // Block comment. WGSL nests them.
            if ( c == '/' and i + 1 < mSource.size() and mSource[i + 1] == '*' )
            {
                int depth = 1;
                i += 2;
                while ( i < mSource.size() and depth > 0 )
                {
                    if ( mSource[i] == '\n' )
                        line++;
                    if ( i + 1 < mSource.size() and mSource[i] == '/' and mSource[i + 1] == '*' )
                    {
                        depth++;
                        i += 2;
                        continue;
                    }
                    if ( i + 1 < mSource.size() and mSource[i] == '*' and mSource[i + 1] == '/' )
                    {
                        depth--;
                        i += 2;
                        continue;
                    }
                    i++;
                }
                continue;
            }

            if ( IsIdentStart( c ) )
            {
                const size_t start = i;
                while ( i < mSource.size() and IsIdentChar( mSource[i] ) )
                    i++;
                mTokens.push_back( { Token::Kind::Ident,
                                     std::string( mSource.substr( start, i - start ) ), line } );
                continue;
            }

            if ( std::isdigit( (unsigned char)c ) )
            {
                const size_t start = i;
                while ( i < mSource.size() and
                        ( std::isalnum( (unsigned char)mSource[i] ) or mSource[i] == '.' ) )
                    i++;
                mTokens.push_back( { Token::Kind::Number,
                                     std::string( mSource.substr( start, i - start ) ), line } );
                continue;
            }

            mTokens.push_back( { Token::Kind::Punct, std::string( 1, c ), line } );
            i++;
        }
    }

    std::string_view mSource;
    std::vector<Token> mTokens;
    std::vector<std::pair<uint32_t, std::string>> mComments;
    size_t mPos = 0;
    Token mEnd;
};


std::optional<uint32_t> ParseUint( std::string_view text )
{
    // WGSL number suffixes: 4u, 4i.
    while ( not text.empty() and ( text.back() == 'u' or text.back() == 'i' ) )
        text.remove_suffix( 1 );
    uint32_t value = 0;
    const auto result = std::from_chars( text.data(), text.data() + text.size(), value );
    if ( result.ec != std::errc() or result.ptr != text.data() + text.size() )
        return std::nullopt;
    return value;
}

std::optional<ScalarType> ScalarFromName( std::string_view name )
{
    if ( name == "f32" ) return ScalarType::F32;
    if ( name == "i32" ) return ScalarType::I32;
    if ( name == "u32" ) return ScalarType::U32;
    if ( name == "bool" ) return ScalarType::Bool;
    if ( name == "f16" ) return ScalarType::F16;
    return std::nullopt;
}

uint32_t ScalarSize( ScalarType type )
{
    return type == ScalarType::F16 ? 2u : 4u;
}

}


bool Type::IsHostShareable() const
{
    // bool has no defined size in a buffer, so it cannot appear in one. Shaders
    // use u32 instead, which is what the engine writes.
    if ( mKind == TypeKind::Scalar and mScalar == ScalarType::Bool )
        return false;
    if ( mKind == TypeKind::Vector or mKind == TypeKind::Matrix )
        return mScalar != ScalarType::Bool;
    return mKind == TypeKind::Scalar or mKind == TypeKind::Array or mKind == TypeKind::Struct;
}


uint32_t AlignOf( const Type& type, const Module& module )
{
    switch ( type.mKind )
    {
        case TypeKind::Scalar:
            return ScalarSize( type.mScalar );

        case TypeKind::Vector:
        {
            const uint32_t component = ScalarSize( type.mScalar );
            // vec3 aligns like vec4 even though it is only three components.
            return ( type.mRows == 2 ? 2u : 4u ) * component;
        }

        case TypeKind::Matrix:
        {
            Type column;
            column.mKind = TypeKind::Vector;
            column.mScalar = type.mScalar;
            column.mRows = type.mRows;
            return AlignOf( column, module );
        }

        case TypeKind::Array:
        {
            if ( const Struct* element = module.FindStruct( type.mName ) )
                return element->mAlign;
            Type element = type;
            element.mKind = TypeKind::Unknown;
            // Element alignment is resolved by the parser for built-in element
            // types; an unresolved one falls back to the uniform minimum.
            return 16;
        }

        case TypeKind::Struct:
        {
            if ( const Struct* s = module.FindStruct( type.mName ) )
                return s->mAlign;
            return 16;
        }

        default:
            return 0;
    }
}

uint32_t SizeOf( const Type& type, const Module& module )
{
    switch ( type.mKind )
    {
        case TypeKind::Scalar:
            return ScalarSize( type.mScalar );

        case TypeKind::Vector:
            return type.mRows * ScalarSize( type.mScalar );

        case TypeKind::Matrix:
        {
            Type column;
            column.mKind = TypeKind::Vector;
            column.mScalar = type.mScalar;
            column.mRows = type.mRows;
            // Each column is padded up to its own alignment, which is why a
            // mat3x3<f32> occupies 48 bytes and not 36.
            const uint32_t stride = RoundUp( AlignOf( column, module ), SizeOf( column, module ) );
            return type.mColumns * stride;
        }

        case TypeKind::Array:
        {
            if ( type.mArrayCount == 0 )
                return 0; // runtime-sized
            uint32_t elementAlign = 16;
            uint32_t elementSize = 16;
            if ( const Struct* element = module.FindStruct( type.mName ) )
            {
                elementAlign = element->mAlign;
                elementSize = element->mSize;
            }
            // In the uniform address space an array stride is a multiple of 16.
            const uint32_t stride = RoundUp( std::max( elementAlign, 16u ), elementSize );
            return type.mArrayCount * stride;
        }

        case TypeKind::Struct:
        {
            if ( const Struct* s = module.FindStruct( type.mName ) )
                return s->mSize;
            return 0;
        }

        default:
            return 0;
    }
}


const StructMember* Struct::FindMember( std::string_view name ) const
{
    auto it = std::find_if( mMembers.begin(), mMembers.end(),
                            [name]( const StructMember& m ) { return m.mName == name; } );
    return it == mMembers.end() ? nullptr : &*it;
}

const Struct* Module::FindStruct( std::string_view name ) const
{
    auto it = std::find_if( mStructs.begin(), mStructs.end(),
                            [name]( const Struct& s ) { return s.mName == name; } );
    return it == mStructs.end() ? nullptr : &*it;
}

const Binding* Module::FindBinding( std::string_view name ) const
{
    auto it = std::find_if( mBindings.begin(), mBindings.end(),
                            [name]( const Binding& b ) { return b.mName == name; } );
    return it == mBindings.end() ? nullptr : &*it;
}

const Binding* Module::FindBindingAt( uint32_t group, uint32_t binding ) const
{
    auto it = std::find_if( mBindings.begin(), mBindings.end(),
                            [group, binding]( const Binding& b )
                            { return b.mGroup == group and b.mBinding == binding; } );
    return it == mBindings.end() ? nullptr : &*it;
}


namespace
{

// Attributes that appear before a declaration or a struct member.
struct Attributes
{
    std::optional<uint32_t> mGroup;
    std::optional<uint32_t> mBinding;
    std::optional<uint32_t> mAlign;
    std::optional<uint32_t> mSize;
    std::optional<uint32_t> mLocation;
    bool mBuiltin = false;
};

Attributes ParseAttributes( Lexer& lexer )
{
    Attributes attributes;
    while ( lexer.PeekIs( "@" ) )
    {
        lexer.Next();
        const std::string name = lexer.Next().mText;

        std::vector<std::string> args;
        if ( lexer.Accept( "(" ) )
        {
            while ( not lexer.AtEnd() and not lexer.PeekIs( ")" ) )
            {
                if ( lexer.PeekIs( "," ) )
                {
                    lexer.Next();
                    continue;
                }
                args.push_back( lexer.Next().mText );
            }
            lexer.Accept( ")" );
        }

        const auto firstArg = args.empty() ? std::nullopt : ParseUint( args[0] );
        if ( name == "group" )         attributes.mGroup = firstArg;
        else if ( name == "binding" )  attributes.mBinding = firstArg;
        else if ( name == "align" )    attributes.mAlign = firstArg;
        else if ( name == "size" )     attributes.mSize = firstArg;
        else if ( name == "location" ) attributes.mLocation = firstArg;
        else if ( name == "builtin" )  attributes.mBuiltin = true;
    }
    return attributes;
}

// Reads a type, including its generic arguments, and records how it was spelled.
Type ParseType( Lexer& lexer )
{
    Type type;
    const Token name = lexer.Next();
    type.mSpelling = name.mText;

    if ( const auto scalar = ScalarFromName( name.mText ) )
    {
        type.mKind = TypeKind::Scalar;
        type.mScalar = *scalar;
        type.mRows = 1;
        return type;
    }

    const std::string_view text = name.mText;

    // vecN<T>
    if ( text.size() == 4 and text.starts_with( "vec" ) and std::isdigit( (unsigned char)text[3] ) )
    {
        type.mKind = TypeKind::Vector;
        type.mRows = (uint32_t)( text[3] - '0' );
        type.mScalar = ScalarType::F32;
        if ( lexer.Accept( "<" ) )
        {
            const Token component = lexer.Next();
            type.mSpelling += "<" + component.mText + ">";
            if ( const auto scalar = ScalarFromName( component.mText ) )
                type.mScalar = *scalar;
            lexer.Accept( ">" );
        }
        return type;
    }

    // matCxR<T>
    if ( text.size() == 6 and text.starts_with( "mat" ) and text[4] == 'x' )
    {
        type.mKind = TypeKind::Matrix;
        type.mColumns = (uint32_t)( text[3] - '0' );
        type.mRows = (uint32_t)( text[5] - '0' );
        type.mScalar = ScalarType::F32;
        if ( lexer.Accept( "<" ) )
        {
            const Token component = lexer.Next();
            type.mSpelling += "<" + component.mText + ">";
            if ( const auto scalar = ScalarFromName( component.mText ) )
                type.mScalar = *scalar;
            lexer.Accept( ">" );
        }
        return type;
    }

    if ( text == "array" )
    {
        type.mKind = TypeKind::Array;
        if ( lexer.Accept( "<" ) )
        {
            const Token element = lexer.Next();
            type.mName = element.mText;
            type.mSpelling += "<" + element.mText;
            // The element may itself be generic, e.g. array<vec4<f32>, 4>.
            int depth = 0;
            while ( not lexer.AtEnd() )
            {
                if ( lexer.PeekIs( "<" ) ) depth++;
                if ( lexer.PeekIs( ">" ) )
                {
                    if ( depth == 0 )
                        break;
                    depth--;
                }
                if ( depth == 0 and lexer.PeekIs( "," ) )
                {
                    lexer.Next();
                    const Token count = lexer.Next();
                    type.mSpelling += ", " + count.mText;
                    if ( const auto value = ParseUint( count.mText ) )
                        type.mArrayCount = *value;
                    continue;
                }
                type.mSpelling += lexer.Next().mText;
            }
            lexer.Accept( ">" );
            type.mSpelling += ">";
        }
        return type;
    }

    if ( text.starts_with( "texture_" ) )
    {
        type.mKind = TypeKind::Texture;
        if ( lexer.Accept( "<" ) )
        {
            std::string inner;
            while ( not lexer.AtEnd() and not lexer.PeekIs( ">" ) )
                inner += lexer.Next().mText;
            lexer.Accept( ">" );
            type.mSpelling += "<" + inner + ">";
        }
        return type;
    }

    if ( text == "sampler" or text == "sampler_comparison" )
    {
        type.mKind = TypeKind::Sampler;
        return type;
    }

    // Anything else is a named type: a struct, or an alias of one.
    type.mKind = TypeKind::Struct;
    type.mName = name.mText;
    return type;
}

// Skips a balanced brace block, used to step over function bodies.
void SkipBraceBlock( Lexer& lexer )
{
    if ( not lexer.Accept( "{" ) )
        return;
    int depth = 1;
    while ( not lexer.AtEnd() and depth > 0 )
    {
        if ( lexer.PeekIs( "{" ) ) depth++;
        if ( lexer.PeekIs( "}" ) ) depth--;
        lexer.Next();
    }
}

void SkipToSemicolon( Lexer& lexer )
{
    while ( not lexer.AtEnd() and not lexer.PeekIs( ";" ) )
        lexer.Next();
    lexer.Accept( ";" );
}

AddressSpace AddressSpaceFromName( std::string_view name )
{
    if ( name == "uniform" )   return AddressSpace::Uniform;
    if ( name == "storage" )   return AddressSpace::Storage;
    if ( name == "workgroup" ) return AddressSpace::Workgroup;
    if ( name == "private" )   return AddressSpace::Private;
    return AddressSpace::None;
}

void LayoutStruct( Struct& s, const Module& module )
{
    uint32_t offset = 0;
    uint32_t maxAlign = 1;

    for ( auto& member : s.mMembers )
    {
        const uint32_t naturalAlign = AlignOf( member.mType, module );
        const uint32_t naturalSize = SizeOf( member.mType, module );

        member.mAlign = member.mExplicitAlign.value_or( naturalAlign );
        member.mSize = member.mExplicitSize.value_or( naturalSize );

        if ( member.mAlign == 0 )
            member.mAlign = 1;

        offset = RoundUp( member.mAlign, offset );
        member.mOffset = offset;
        offset += member.mSize;
        maxAlign = std::max( maxAlign, member.mAlign );
    }

    // A struct in the uniform address space is aligned to at least 16.
    s.mAlign = std::max( maxAlign, 16u );
    s.mSize = RoundUp( s.mAlign, offset );
}

}


Module Parse( std::string_view source )
{
    Module module;
    Lexer lexer( source );

    while ( not lexer.AtEnd() )
    {
        const Attributes attributes = ParseAttributes( lexer );
        const Token token = lexer.Peek();

        if ( token.mKind == Token::Kind::End )
            break;

        if ( token.mText == "struct" )
        {
            lexer.Next();
            Struct s;
            s.mName = lexer.Next().mText;

            if ( not lexer.Accept( "{" ) )
            {
                module.mErrors.push_back( "struct " + s.mName + ": expected '{'" );
                continue;
            }

            while ( not lexer.AtEnd() and not lexer.PeekIs( "}" ) )
            {
                const Attributes memberAttributes = ParseAttributes( lexer );
                if ( lexer.PeekIs( "}" ) )
                    break;

                StructMember member;
                const Token nameToken = lexer.Next();
                member.mName = nameToken.mText;
                member.mExplicitAlign = memberAttributes.mAlign;
                member.mExplicitSize = memberAttributes.mSize;

                if ( not lexer.Accept( ":" ) )
                {
                    module.mErrors.push_back( "struct " + s.mName + "." + member.mName +
                                              ": expected ':'" );
                    SkipToSemicolon( lexer );
                    continue;
                }

                member.mType = ParseType( lexer );
                // A comment on the member's own line, for caller annotations.
                member.mTrailingComment = lexer.CommentOnLine( nameToken.mLine );

                lexer.Accept( "," );
                lexer.Accept( ";" );
                s.mMembers.push_back( std::move( member ) );
            }
            lexer.Accept( "}" );
            lexer.Accept( ";" );

            // Laid out against the structs already known, so a member whose type
            // is another struct resolves - which requires declaration before use,
            // as WGSL demands anyway.
            LayoutStruct( s, module );
            module.mStructs.push_back( std::move( s ) );
            continue;
        }

        if ( token.mText == "var" )
        {
            lexer.Next();
            Binding binding;
            binding.mGroup = attributes.mGroup.value_or( 0 );
            binding.mBinding = attributes.mBinding.value_or( 0 );

            if ( lexer.Accept( "<" ) )
            {
                binding.mAddressSpace = AddressSpaceFromName( lexer.Next().mText );
                // An access mode may follow, e.g. var<storage, read>.
                while ( not lexer.AtEnd() and not lexer.PeekIs( ">" ) )
                    lexer.Next();
                lexer.Accept( ">" );
            }

            binding.mName = lexer.Next().mText;
            if ( lexer.Accept( ":" ) )
                binding.mType = ParseType( lexer );

            SkipToSemicolon( lexer );

            // Only the ones actually reachable from a bind group are interesting;
            // a module-scope private var is not one.
            const bool isResource = attributes.mGroup.has_value() and
                                    attributes.mBinding.has_value();
            if ( isResource )
                module.mBindings.push_back( std::move( binding ) );
            continue;
        }

        if ( token.mText == "fn" )
        {
            lexer.Next();
            // Name, parameter list and return type, then the body.
            while ( not lexer.AtEnd() and not lexer.PeekIs( "{" ) )
                lexer.Next();
            SkipBraceBlock( lexer );
            continue;
        }

        if ( token.mText == "alias" or token.mText == "const" or
             token.mText == "override" or token.mText == "let" )
        {
            lexer.Next();
            SkipToSemicolon( lexer );
            continue;
        }

        // Anything else at module scope is not something this needs to model.
        lexer.Next();
    }

    return module;
}


std::string_view ToString( ScalarType type )
{
    switch ( type )
    {
        case ScalarType::Bool: return "bool";
        case ScalarType::I32:  return "i32";
        case ScalarType::U32:  return "u32";
        case ScalarType::F32:  return "f32";
        case ScalarType::F16:  return "f16";
    }
    return "?";
}

std::string_view ToString( TypeKind kind )
{
    switch ( kind )
    {
        case TypeKind::Unknown: return "unknown";
        case TypeKind::Scalar:  return "scalar";
        case TypeKind::Vector:  return "vector";
        case TypeKind::Matrix:  return "matrix";
        case TypeKind::Array:   return "array";
        case TypeKind::Struct:  return "struct";
        case TypeKind::Texture: return "texture";
        case TypeKind::Sampler: return "sampler";
    }
    return "?";
}

std::string_view ToString( AddressSpace space )
{
    switch ( space )
    {
        case AddressSpace::None:      return "handle";
        case AddressSpace::Uniform:   return "uniform";
        case AddressSpace::Storage:   return "storage";
        case AddressSpace::Workgroup: return "workgroup";
        case AddressSpace::Private:   return "private";
    }
    return "?";
}

}
