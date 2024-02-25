#pragma once
#include "engine/serialization/types_serialization.hpp"
#include <nlohmann/json.hpp>

namespace bubble
{
void to_json( json& j, const uvec2& v ) { j = json{ v[0], v[1] }; }
void from_json( const json& j, uvec2& v ) { v = uvec2( j[0], j[1] ); }
void to_json( json& j, const uvec3& v ) { j = json{ v[0], v[1], v[2] }; }
void from_json( const json& j, uvec3& v ) { v = uvec3( j[0], j[1], j[2] ); }
void to_json( json& j, const uvec4& v ) { j = json{ v[0], v[1], v[2], v[3] }; }
void from_json( const json& j, uvec4& v ) { v = uvec4( j[0], j[1], j[2], j[3] ); }

void to_json( json& j, const ivec2& v ) { j = json{ v[0], v[1] }; }
void from_json( const json& j, ivec2& v ) { v = ivec2( j[0], j[1] ); }
void to_json( json& j, const ivec3& v ) { j = json{ v[0], v[1], v[2] }; }
void from_json( const json& j, ivec3& v ) { v = ivec3( j[0], j[1], j[2] ); }
void to_json( json& j, const ivec4& v ) { j = json{ v[0], v[1], v[2], v[3] }; }
void from_json( const json& j, ivec4& v ) { v = ivec4( j[0], j[1], j[2], j[3] ); }

void to_json( json& j, const vec2& v ) { j = json{ v[0], v[1] }; }
void from_json( const json& j, vec2& v ) { v = vec2( j[0], j[1] ); }
void to_json( json& j, const vec3& v ) { j = json{ v[0], v[1], v[2] }; }
void from_json( const json& j, vec3& v ) { v = vec3( j[0], j[1], j[2] ); }
void to_json( json& j, const vec4& v ) { j = json{ v[0], v[1], v[2], v[3] }; }
void from_json( const json& j, vec4& v ) { v = vec4( j[0], j[1], j[2], j[3] ); }

void to_json( json& j, const mat3& m )
{
    for ( i32 row = 0; row < m.length(); row++ )
        for ( i32 col = 0; col < m.length(); col++ )
            j.push_back( m[row][col] );
}

void from_json( const json& j, mat3& m )
{
    for ( i32 row = 0; row < m.length(); row++ )
        for ( i32 col = 0; col < m.length(); col++ )
            m[row][col] = j[row * m.length() + col];
}

void to_json( json& j, const mat4& m )
{
    for ( i32 row = 0; row < m.length(); row++ )
        for ( i32 col = 0; col < m.length(); col++ )
            j.push_back( m[row][col] );
}

void from_json( const json& j, mat4& m )
{
    for ( i32 row = 0; row < m.length(); row++ )
        for ( i32 col = 0; col < m.length(); col++ )
            m[row][col] = j[row * m.length() + col];
}

}
