#pragma once
#include "engine/types/glm.hpp"
#include "engine/types/json.hpp"

namespace glm
{
using namespace bubble;

void to_json( json& j, const uvec2& v );
void from_json( const json& j, uvec2& v );
void to_json( json& j, const uvec3& v );
void from_json( const json& j, uvec3& v );

void to_json( json& j, const ivec2& v );
void from_json( const json& j, ivec2& v );
void to_json( json& j, const ivec3& v );
void from_json( const json& j, ivec3& v );

void to_json( json& j, const vec2& v );
void from_json( const json& j, vec2& v );
void to_json( json& j, const vec3& v );
void from_json( const json& j, vec3& v );

void to_json( json& j, const mat3& m );
void from_json( const json& j, mat3& m );
void to_json( json& j, const mat4& m );
void from_json( const json& j, mat4& m );

}

namespace CPM_GLM_AABB_NS
{
using namespace bubble;

void to_json( json& j, const AABB& bbox );
void from_json( const json& j, AABB& bbox );
}
