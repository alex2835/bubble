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
// A color is [r, g, b, a]; an asset its id in hex; an entity its path.
Json ValueToJson( const Value& value, const ValueType& type );

// The error says where in the text and where in the value:
// "12:9: light.brightness: a number expected, got "bright"". A field the
// JSON leaves out is left out of the record too: whoever makes an object
// from it keeps that field's default.
expected<Value, string> TryValueFromJson( const Json& json, const ValueType& type, string_view name );
}
