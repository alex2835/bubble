#pragma once
#include "bubble/data/json.hpp"
#include "bubble/data/value.hpp"

namespace bubble
{
// A Value in a file. Where its declaration says what it is - a C++ field's
// type - the JSON is plain: 3, "door", [1, 0.5, 0], "/props/door". Where it
// does not (ValueKind::Any), the kinds JSON cannot tell apart carry a tag:
// { "$vec3": [1, 2, 3] }, { "$entity": "/props/door" }. A field's name never
// begins with $, so a tag is never taken for a record.
//
// A color is [r, g, b, a]; an asset its id in hex; an entity its path. A
// number JSON has no word for - NaN, infinity - is an error that says where
// it is, "light.brightness: nan is a number a file cannot hold"; `name` is
// what that path starts with.
expected<Json, string> TryValueToJson( const Value& value, const ValueType& type, string_view name );

// The error says where in the text and where in the value:
// "12:9: light.brightness: a number expected, got "bright"". A vec3's or a
// color's number past what a float holds is refused too. A field the
// JSON leaves out is left out of the record too: whoever makes an object
// from it keeps that field's default.
expected<Value, string> TryValueFromJson( const Json& json, const ValueType& type, string_view name );
}
