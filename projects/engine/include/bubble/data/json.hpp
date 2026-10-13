#pragma once
#include "bubble/types/containers.hpp"
#include "bubble/types/number.hpp"
#include "bubble/types/opt_ref.hpp"
#include "bubble/types/string.hpp"
#include "bubble/types/utility.hpp"
#include "bubble/types/variant.hpp"

namespace bubble
{
// JSON as text and as a tree: what level and prefab files are written in.
// Small on purpose - null, bool, number, string, array, object - since what
// a field holds and what it means is Value's and reflection's business, not
// the file's. An object keeps its members in order, so a file saved twice
// is the same file, and a diff shows what changed.
class Json
{
public:
    using Array = vector<Json>;
    struct Member;
    using Object = vector<Member>;

    // null.
    Json();
    Json( bool value );
    Json( f64 value );
    Json( string value );
    Json( Array value );
    Json( Object value );
    // A literal would turn into a bool.
    Json( const char* ) = delete;

    Json( const Json& );
    Json( Json&& ) noexcept;
    Json& operator=( const Json& );
    Json& operator=( Json&& ) noexcept;
    ~Json();

    // The text as a tree. The error says where - line and column, both from
    // 1, the column in code points - and what was there instead:
    // "3:14: , or } expected, got x".
    static expected<Json, string> TryParse( string_view text );
    // Indented with two spaces; an array of numbers, bools and strings on
    // one line, as a vec3 reads best. A number that is not finite, which
    // JSON has no word for, is written null.
    string Write() const;

    bool IsNull() const;
    template <typename T>
    bool Is() const
    {
        return holds_alternative<T>( mData );
    }

    template <typename T>
    OptRef<const T> TryAs() const
    {
        const T* value = get_if<T>( &mData );
        return value ? OptRef<const T>( *value ) : nullopt;
    }

    // An object's member by key; nothing for another key or a non-object.
    OptRef<const Json> TryFind( string_view key ) const;

    // Where this value starts in the text it was parsed from, "3:14"; for
    // messages about what it holds. Empty for one made in code.
    string Where() const;

    // For messages: null, true, 2.5, "door", an array of 3, an object.
    string Describe() const;

private:
    variant<monostate, bool, f64, string, Array, Object> mData;
    // From 1; 0 for a value made in code.
    u32 mLine = 0;
    u32 mColumn = 0;

    friend class JsonParser;
};

struct Json::Member
{
    string mKey;
    Json mValue;
};
}
