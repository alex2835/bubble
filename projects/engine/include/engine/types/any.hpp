#pragma once
#include <sol/forward.hpp>
#include "engine/types/json.hpp"

namespace bubble
{
class Project;
class ScriptingEngine;
class Shader;

using Any = sol::lua_value;
using Table = sol::table;
using Object = sol::object;

bool IsClass( const Table& tbl );
bool IsArray( const Table& tbl );
string AnyValueToString( const Any& value );
void PrintAnyValue( const Any& value );

json SaveAnyValue( const Any& v );
Any LoadAnyValue( ScriptingEngine& se, const json& j );

Any AnyDeepCopy( const Any& any );
Scope<Any> AnyDeepCopy( const Scope<Any>& any );

void DrawFieldsAdding( Project& project, Table& table, string_view scopeName, bool frozen = false );
Any DrawAnyValue( Project& project, string_view name, Any any, bool frozen = false );

// Packs a shader's own uniform values into the byte block its
// UserUniforms struct describes, ready to be pushed to the GPU.
void PackShaderUniforms( const Shader& shader, const Table& uniforms, vector<u8>& block );

}