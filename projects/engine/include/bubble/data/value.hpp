#pragma once
#include "bubble/assets/asset_id.hpp"
#include "bubble/core/color.hpp"
#include "bubble/core/name.hpp"
#include "bubble/core/text.hpp"
#include "bubble/scene/entity_path.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/math.hpp"
#include "bubble/types/opt_ref.hpp"
#include "bubble/types/pointer.hpp"
#include "bubble/types/variant.hpp"

namespace bubble
{
struct ValueField;

// A value as a file keeps it: a prop of a script, a parameter of a material.
// A closed set - bool, number, text, vec3, color, name, asset, entity, a list
// and a record with its fields in order - and no free dictionaries: a list
// of records stands in for one. What the value has to be is not in it but in
// its declaration, a ValueType; Check holds one against the other.
class Value
{
public:
    using List = vector<Value>;
    using Record = vector<ValueField>;

    // false.
    Value();
    Value( bool value );
    Value( f64 value );
    Value( int value );
    Value( Text value );
    Value( vec3 value );
    Value( Color value );
    Value( Name value );
    Value( AssetId value );
    Value( EntityPath value );
    Value( List value );
    Value( Record value );
    // A literal would turn into a bool: text is Text::TryFrom's.
    Value( const char* ) = delete;

    Value( const Value& );
    Value( Value&& ) noexcept;
    Value& operator=( const Value& );
    Value& operator=( Value&& ) noexcept;
    ~Value();

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

    template <typename T>
    OptRef<T> TryAs()
    {
        T* value = get_if<T>( &mData );
        return value ? OptRef<T>( *value ) : nullopt;
    }

    // What it holds, as a word: "bool", "number", "text", "vec3", "color",
    // "name", "asset", "entity", "list", "record". Two values of one kind
    // can stand in for each other.
    string_view Kind() const;

    // For messages: true, 2.5, "door", vec3( 1, 2, 3 ), a list of 3.
    string Describe() const;

    friend bool operator==( const Value& a, const Value& b );

private:
    variant<bool, f64, Text, vec3, Color, Name, AssetId, EntityPath, List, Record> mData;
};

struct ValueField
{
    Name mName;
    Value mValue;

    friend bool operator==( const ValueField&, const ValueField& ) = default;
};

enum class ValueKind : u8
{
    Bool,
    Number,
    // A number that has to be whole: Luau has only number, the declaration
    // says which.
    Integer,
    Text,
    Vec3,
    Color,
    Name,
    Asset,
    Entity,
    List,
    Record,
    // Whatever a Value holds, its kind told by the value itself: a field of
    // type Value, such as a script's props before the script declares them.
    // In a file such a value says its kind where JSON alone would not.
    Any,
};

struct ValueTypeField;

// What a Value has to be, as a declaration says it: a kind, and for a list
// the type of every element, for a record its fields in order.
struct ValueType
{
    ValueKind mKind = ValueKind::Bool;
    // A list's elements; shared, as types are written once and copied often.
    Ref<const ValueType> mElement;
    vector<ValueTypeField> mFields;

    // A kind with nothing more to say: not a list, not a record.
    static ValueType Of( ValueKind kind );
    static ValueType ListOf( ValueType element );
    static ValueType RecordOf( vector<ValueTypeField> fields );

    // For messages: "an integer", "a list of vec3", "a record (count, delay)".
    string Describe() const;
};

struct ValueTypeField
{
    Name mName;
    ValueType mType;
};

// An f32 as the f64 a person would write for it: 0.1f becomes 0.1, not
// 0.10000000149011612 - the shortest decimal that reads back as the same
// f32. What a float field turns into in a Value and in a file.
f64 WidenFloat( f32 value );

// Whether `value` is what `type` says. The error says where, as a path from
// `name`, and what it got: "waves[2].count: an integer expected, got 2.5".
// Positions in a list count from 1, as in Luau.
expected<void, string> TryCheck( const Value& value, const ValueType& type, string_view name );
}
