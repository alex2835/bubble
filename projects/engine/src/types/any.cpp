#include "engine/pch/pch.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "engine/types/any.hpp"
#include "engine/scripting/scripting_engine.hpp"
#include "engine/types/string.hpp"
#include "engine/types/json.hpp"
#include "engine/utils/imgui_utils.hpp"
#include "engine/scene/scene.hpp"
#include "engine/renderer/texture.hpp"
#include "engine/renderer/shader.hpp"
#include "engine/loader/loader.hpp"
#include "engine/project/project.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>
#include <imgui.h>
#include <print>

namespace bubble
{
    
bool IsClass( const Table& tbl )
{
    if ( not tbl.valid() )
        return false;
    sol::object meta = tbl[sol::metatable_key];
    if ( meta == sol::nil )
        return false;
    return meta.as<sol::table>()["__name"] != sol::nil;
}

bool IsArray( const Table& tbl )
{
    if ( tbl.empty() )
        return false;

    for ( auto& [key, value] : tbl )
    {
        if ( not key.is<int>() or key.as<int>() < 1 )
            return false;
    }
    return true;
}

string AnyValueToString( const Any& value )
{
    if ( value.is<sol::nil_t>() )
        return "nil";
    else if ( value.is<Table>() )
    {
        const auto& table = value.as<Table>();
        if ( IsClass( table ) )
        {
            auto metatable = table[sol::metatable_key];
            sol::optional<sol::function> tostring_fn = metatable["__tostring"];
            if ( tostring_fn != sol::nil )
            {
                std::string className = metatable["__name"].get_or( std::string( "user type" ) );
                std::string objectString = ( *tostring_fn )( table );
                return std::format( "({}){}", className, objectString );
            }
            else
                return "(user class) no __tostring";
        }
        else if ( IsArray( table ) ) // array
        {
            string result = "[";
            for ( auto& [idx, val] : table )
                result += AnyValueToString( val ) + ", ";
            result += "]";
            return result;
        }
        else // map
        {
            string result = "{";
            for ( auto& [key, val] : table )
                result += AnyValueToString( key ) + " : " + AnyValueToString( val ) + "; ";
            result += "}";
            return result;
        }
    }
    else if ( value.is<int>() )
        return std::format( "(int)'{}'", value.as<int>() );
    else if ( value.is<float>() )
        return std::format( "(float)'{}'", value.as<float>() );
    else if ( value.is<std::string>() )
        return std::format( "(string)'{}'", value.as<std::string>() );
    else if ( value.is<bool>() )
        return std::format( "(bool)'{}'", value.as<bool>() );
    else if ( value.is<vec2>() )
    {
        const auto& v = value.as<vec2>();
        return std::format( "(vec2)'[{},{}]'", v.x, v.y );
    }
    else if ( value.is<vec3>() )
    {
        const auto& v = value.as<vec3>();
        return std::format( "(vec3)'[{},{},{}]'", v.x, v.y, v.z );
    }
    else if ( value.is<vec4>() )
    {
        const auto& v = value.as<vec4>();
        return std::format( "(vec4)'[{},{},{},{}]'", v.x, v.y, v.z, v.w );
    }
    else if ( value.is<mat3>() )
        return "(mat3)";
    else if ( value.is<mat4>() )
        return "(mat4)";
    else if ( value.is<Entity>() )
        return std::format( "(Entity)'{}'", (size_t)value.as<Entity>() );
    else if ( value.is<Ref<Texture2D>>() )
    {
        const auto& texture = value.as<Ref<Texture2D>>();
        return std::format( "(Texture2D)'{}'", texture ? texture->mPath.string() : "null" );
    }
    else
        return "(unknown)";
}

void PrintAnyValue( const Any& value )
{
    std::println( "{}", AnyValueToString( value ) );
}

json SaveAnyValue( const Any& v )
{
    if ( v.is<int>() )
        return v.as<int>();
    else if ( v.is<float>() )
        return v.as<float>();
    else if ( v.is<string>() )
        return v.as<string>();
    else if ( v.is<bool>() )
        return v.as<bool>();
    else if ( v.is<Entity>() )
    {
        json j;
        j["__type"] = "Entity";
        j["id"] = (size_t)v.as<Entity>();
        return j;
    }
    else if ( v.is<Ref<Texture2D>>() )
    {
        json j;
        j["__type"] = "Texture2D";
        auto& tex = v.as<Ref<Texture2D>>();
        j["path"] = tex ? tex->mPath.string() : "";
        return j;
    }
    else if ( v.is<vec2>() )
    {
        const auto& val = v.as<vec2>();
        return json{ { "__type", "vec2" }, { "x", val.x }, { "y", val.y } };
    }
    else if ( v.is<vec3>() )
    {
        const auto& val = v.as<vec3>();
        return json{ { "__type", "vec3" }, { "x", val.x }, { "y", val.y }, { "z", val.z } };
    }
    else if ( v.is<vec4>() )
    {
        const auto& val = v.as<vec4>();
        return json{ { "__type", "vec4" }, { "x", val.x }, { "y", val.y }, { "z", val.z }, { "w", val.w } };
    }
    else if ( v.is<mat3>() )
    {
        const auto& val = v.as<mat3>();
        json j;
        j["__type"] = "mat3";
        for ( int i = 0; i < 3; i++ )
            j["cols"].push_back( { val[i].x, val[i].y, val[i].z } );
        return j;
    }
    else if ( v.is<mat4>() )
    {
        const auto& val = v.as<mat4>();
        json j;
        j["__type"] = "mat4";
        for ( int i = 0; i < 4; i++ )
            j["cols"].push_back( { val[i].x, val[i].y, val[i].z, val[i].w } );
        return j;
    }
    else if ( v.is<Table>() and IsArray( v.as<Table>() ) )
    {
        json j = json::array();
        auto table = v.as<Table>();
        for ( const auto& [k, val] : table )
            j.push_back( SaveAnyValue( val ) );
        return j;
    }
    else if ( v.is<Table>() )
    {
        json j = json::object();
        auto table = v.as<Table>();
        for ( const auto& [k, val] : table )
            j[k.as<string>()] = SaveAnyValue( val );
        return j;
    }
    else
    {
        PrintAnyValue( v );
        throw std::runtime_error( "Value of not supported type" );
    }
}


Any LoadAnyValue( ScriptingEngine& se, const json& j )
{
    if ( j.is_number_integer() )
        return j.get<int>();
    else if ( j.is_number_float() )
        return j.get<float>();
    else if ( j.is_string() )
        return j.get<string>();
    else if ( j.is_boolean() )
        return j.get<bool>();
    else if ( j.is_object() and j.contains( "__type" ) )
    {
        auto type = j["__type"].get<string>();
        if ( type == "Entity" )
        {
            auto id = j["id"].get<size_t>();
            return *(Entity*)&id;
        }
        else if ( type == "Texture2D" )
        {
            auto texPath = j["path"].get<string>();
            if ( texPath.empty() )
                return Ref<Texture2D>{};
            return LoadTexture2D( texPath );
        }
        else if ( type == "vec2" )
            return vec2( j["x"], j["y"] );
        else if ( type == "vec3" )
            return vec3( j["x"], j["y"], j["z"] );
        else if ( type == "vec4" )
            return vec4( j["x"], j["y"], j["z"], j["w"] );
        else if ( type == "mat3" )
        {
            mat3 m;
            for ( int i = 0; i < 3; i++ )
                m[i] = vec3( j["cols"][i][0], j["cols"][i][1], j["cols"][i][2] );
            return m;
        }
        else if ( type == "mat4" )
        {
            mat4 m;
            for ( int i = 0; i < 4; i++ )
                m[i] = vec4( j["cols"][i][0], j["cols"][i][1], j["cols"][i][2], j["cols"][i][3] );
            return m;
        }
    }
    else if ( j.is_array() )
    {
        auto table = se.CreateTable();
        int i = 1;
        for ( const auto& v : j )
            table[i++] = LoadAnyValue( se, v );
        return table;
    }
    else if ( j.is_object() )
    {
        auto table = se.CreateTable();
        for ( const auto& [k, v] : j.items() )
            table[k] = LoadAnyValue( se, v );
        return table;
    }
    throw std::runtime_error( std::format( "Value of not supported type: {}", string( j ) ) );
}


Any AnyDeepCopy( const Any& any )
{
    if ( any.is<Table>() )
    {
        auto table = any.as<Table>();
        sol::state_view lua = table.lua_state();
        auto newTable = lua.create_table();
        for ( auto& [k, v] : table )
            newTable[k] = v.is<Table>() ? AnyDeepCopy( v ) : v;
        return newTable;
    }
    return any;
}

Scope<Any> AnyDeepCopy( const Scope<Any>& any )
{
    BUBBLE_ASSERT( any, "Empty pointer copy" );
    return CreateScope<Any>( AnyDeepCopy( *any ) );
}


void DrawFieldsAdding( Project& project, Table& table, string_view scopeName, bool frozen )
{
    if ( frozen )
        return;

    const auto isEmpty = table.empty();
    bool isArray = IsArray( table );
    int newId = (int)table.size() + 1;

    const bool addValue = ImGui::Button( "+", ImVec2( 20, 20 ) );
    ImGui::SameLine();

    // Per-scope statics keyed by table pointer so different callers don't share state
    static std::unordered_map<const void*, string> sFieldNames;
    static std::unordered_map<const void*, int>    sSelectedTypes;
    const void* key = table.pointer();
    string& fieldName   = sFieldNames[key];
    int&    selectedType = sSelectedTypes[key];

    if ( not isArray )
    {
        ImGui::SetNextItemWidth( 100.0f );
        auto fieldLabel = std::format( "##field_{}", scopeName );
        ImGui::InputText( fieldLabel.c_str(), fieldName );
        ImGui::SameLine();
    }
    if ( auto val = TryParseInt( fieldName );
         isEmpty and val and val >= 1 )
    {
        isArray = true;
        newId = *val;
    }

    ImGui::SetNextItemWidth( 100.0f );
    constexpr string_view types = "Int\0Float\0String\0Bool\0Vec2\0Vec3\0Vec4\0Mat3\0Mat4\0Table\0Texture2D\0Entity\0"sv;
    auto typeLabel = std::format( "##type_{}", scopeName );
    ImGui::Combo( typeLabel.c_str(), &selectedType, types.data() );

    if ( addValue )
    {
        if ( fieldName.empty() )
            return;

        sol::state_view lua = table.lua_state();
        auto entryKey = isArray ? sol::object( lua, sol::in_place, newId )
            : sol::object( lua, sol::in_place, fieldName );

        enum Types { Int, Float, String, Bool, Vec2, Vec3, Vec4, Mat3, Mat4, Table, Texture2D, Entity };
        switch ( selectedType )
        {
            case Int:       table[entryKey] = 0; break;
            case Float:     table[entryKey] = 0.0f; break;
            case String:    table[entryKey] = ""s; break;
            case Bool:      table[entryKey] = false; break;
            case Vec2:      table[entryKey] = vec2( 0 ); break;
            case Vec3:      table[entryKey] = vec3( 0 ); break;
            case Vec4:      table[entryKey] = vec4( 0 ); break;
            case Mat3:      table[entryKey] = mat3( 1 ); break;
            case Mat4:      table[entryKey] = mat4( 1 ); break;
            case Table:     table[entryKey] = lua.create_table(); break;
            case Texture2D: table[entryKey] = Ref<bubble::Texture2D>{}; break;
            case Entity:
            {
                auto entity = project.mScene.CreateEntity();
                table[entryKey] = entity;
                break;
            }
        }
    }
}

Any DrawAnyValue( Project& project, string_view name, Any any, bool frozen )
{
    constexpr auto TABLE_FLAGS = ImGuiTreeNodeFlags_DefaultOpen |
                                 ImGuiTreeNodeFlags_SpanAllColumns |
                                 ImGuiTreeNodeFlags_Framed;

    auto& lua = *project.mScriptingEngine.mLua;

    // Scope all widget IDs under `name` so identical field names in different
    // components (State vs ShaderUniforms) don't collide.
    struct IDGuard
    {
        IDGuard( string_view id ) { ImGui::PushID( id.data(), id.data() + id.size() ); }
        ~IDGuard() { ImGui::PopID(); }
    } idGuard( name );

    ImGui::SetNextItemWidth( 100.0f );
    if ( any.is<sol::nil_t>() )
    {
        ImGui::SameLine();
        ImGui::Text( "(nill)" );
        return any;
    }
    else if ( any.is<int>() )
    {
        auto value = any.as<int>();
        ImGui::DragInt( name.data(), &value );
        ImGui::SameLine();
        ImGui::Text( "(int)" );
        return value;
    }
    else if ( any.is<float>() )
    {
        auto value = any.as<float>();
        ImGui::DragFloat( name.data(), &value );
        ImGui::SameLine();
        ImGui::Text( "(float)" );
        return value;
    }
    else if ( any.is<std::string>() )
    {
        auto value = any.as<string>();
        ImGui::InputText( name.data(), value );
        ImGui::SameLine();
        ImGui::Text( "(string)" );
        return value;
    }
    else if ( any.is<bool>() )
    {
        auto value = any.as<bool>();
        ImGui::Checkbox( name.data(), &value );
        ImGui::SameLine();
        ImGui::Text( "(bool)" );
        return value;
    }
    else if ( any.is<vec2>() )
    {
        auto value = any.as<vec2>();
        ImGui::DragFloat2( name.data(), &value.x );
        ImGui::SameLine();
        ImGui::Text( "(vec2)" );
        return value;
    }
    else if ( any.is<vec3>() )
    {
        auto value = any.as<vec3>();
        ImGui::DragFloat3( name.data(), &value.x );
        ImGui::SameLine();
        ImGui::Text( "(vec3)" );
        return value;
    }
    else if ( any.is<vec4>() )
    {
        auto value = any.as<vec4>();
        ImGui::DragFloat4( name.data(), &value.x );
        ImGui::SameLine();
        ImGui::Text( "(vec4)" );
        return value;
    }
    else if ( any.is<mat3>() )
    {
        auto value = any.as<mat3>();
        ImGui::Text( "%s (mat3)", name.data() );
        for ( int i = 0; i < 3; i++ )
        {
            auto label = std::format( "{}[{}]", name, i );
            ImGui::DragFloat3( label.c_str(), &value[i].x );
        }
        return value;
    }
    else if ( any.is<mat4>() )
    {
        auto value = any.as<mat4>();
        ImGui::Text( "%s (mat4)", name.data() );
        for ( int i = 0; i < 4; i++ )
        {
            auto label = std::format( "{}[{}]", name, i );
            ImGui::DragFloat4( label.c_str(), &value[i].x );
        }
        return value;
    }
    else if ( any.is<Entity>() )
    {
        auto currentEntity = any.as<Entity>();

        // Build list of all entities that have a tag
        vector<Entity> entities;
        vector<string> entityNames;
        project.mScene.ForEach<TagComponent>( [&]( Entity entity, const TagComponent& tag )
        {
            entities.push_back( entity );
            entityNames.push_back( std::format( "[{}] {}", (size_t)entity, tag.mName ) );
        } );

        // Find current selection index
        int selectedIdx = -1;
        for ( int i = 0; i < (int)entities.size(); i++ )
            if ( entities[i] == currentEntity )
            {
                selectedIdx = i;
                break;
            }

        // Build c-string array for combo
        vector<const char*> items;
        items.reserve( entityNames.size() );
        for ( const auto& n : entityNames )
            items.push_back( n.c_str() );

        ImGui::SetNextItemWidth( 150.0f );
        string preview = selectedIdx >= 0 ? entityNames[selectedIdx] : "None";
        if ( ImGui::BeginCombo( name.data(), preview.c_str() ) )
        {
            for ( int i = 0; i < (int)entities.size(); i++ )
            {
                bool isSelected = ( i == selectedIdx );
                if ( ImGui::Selectable( items[i], isSelected ) )
                    currentEntity = entities[i];
                if ( isSelected )
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::Text( "(Entity)" );
        return currentEntity;
    }
    else if ( any.is<Ref<Texture2D>>() )
    {
        auto currentTexture = any.as<Ref<Texture2D>>();

        // Build list from loader
        vector<Ref<Texture2D>> textures;
        vector<string> textureNames;
        for ( const auto& [texPath, tex] : project.mLoader.mTextures )
        {
            textures.push_back( tex );
            textureNames.push_back( texPath.filename().string() );
        }

        // Find current selection index
        int selectedIdx = -1;
        for ( int i = 0; i < (int)textures.size(); i++ )
        {
            if ( textures[i] == currentTexture )
            {
                selectedIdx = i;
                break;
            }
        }

        string preview = selectedIdx >= 0 ? textureNames[selectedIdx] : "None";
        ImGui::SetNextItemWidth( 150.0f );
        if ( ImGui::BeginCombo( name.data(), preview.c_str() ) )
        {
            if ( ImGui::Selectable( "None", selectedIdx == -1 ) )
                currentTexture = nullptr;

            for ( int i = 0; i < (int)textures.size(); i++ )
            {
                bool isSelected = ( i == selectedIdx );
                ImGui::Image( (ImTextureID)textures[i]->ImTextureId(), ImVec2( 24, 24 ) );
                ImGui::SameLine();
                if ( ImGui::Selectable( textureNames[i].c_str(), isSelected ) )
                    currentTexture = textures[i];
                if ( isSelected )
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::Text( "(Texture2D)" );
        if ( currentTexture )
            ImGui::Image( (ImTextureID)currentTexture->ImTextureId(), ImVec2( 64, 64 ) );
        return currentTexture;
    }
    else if ( any.is<Table>() and IsArray( any.as<Table>() ) )
    {
        int i = 0;
        auto table = any.as<Table>();
        if ( ImGui::TreeNodeEx( "##array", TABLE_FLAGS, "%s (array)", name.data() ) )
        {
            for ( auto& [k, v] : table )
            {
                ImGui::PushID( ( i32 )reinterpret_cast<i64>( table.pointer() ) + i );

                if ( !frozen && ImGui::Button( "-" ) )
                {
                    table[k] = sol::nil;
                    ImGui::PopID();
                    continue;
                }
                if ( !frozen ) ImGui::SameLine();

                auto entryName = std::to_string( k.as<int>() );
                table[k] = DrawAnyValue( project, entryName, v.as<Any>(), frozen );

                ImGui::Separator();
                ImGui::PopID();
                i++;
            }
            DrawFieldsAdding( project, table, name, frozen );
            ImGui::TreePop();
        }
    }
    else if ( any.is<Table>() )
    {
        int i = 0;
        auto table = any.as<Table>();
        if ( ImGui::TreeNodeEx( "##table", TABLE_FLAGS, "%s (table)", name.data() ) )
        {
            for ( auto& [k, v] : table )
            {
                ImGui::PushID( ( i32 )reinterpret_cast<i64>( table.pointer() ) + i );

                if ( !frozen && ImGui::Button( "-" ) )
                {
                    table[k] = sol::nil;
                    ImGui::PopID();
                    continue;
                }
                if ( !frozen ) ImGui::SameLine();

                auto entryName = k.as<string>();
                table[k] = DrawAnyValue( project, entryName, v.as<Any>(), frozen );

                ImGui::Separator();
                ImGui::PopID();
                i++;
            }
            DrawFieldsAdding( project, table, name, frozen );
            ImGui::TreePop();
        }
    }
    else
    {
        BUBBLE_ASSERT( false, "Invalid any value" );
        throw std::runtime_error( "DrawAny(): Invalid Any value type" );
    }
    return any;
}


// Packs a shader's own uniform values into its UserUniforms block.
//
// This replaced a walk over the descriptor set that pushed each value straight
// at the program with glUniform*. WebGPU has no loose uniforms: a value has to
// be written into a buffer at the byte offset the shader gives it, which is what
// the WGSL reflection in the shader loader records in mUniformOffsets.
//
// The contract above this is unchanged. Values still arrive in a Lua table keyed
// by name, a value of the wrong type is still skipped rather than coerced, and a
// uniform the table has no entry for keeps whatever the block already holds.
void PackShaderUniforms( const Shader& shader, const Table& uniforms, vector<u8>& block )
{
    block.assign( shader.mUserUniformSize, 0 );
    if ( block.empty() )
        return;

    const auto write = [&]( u32 offset, const void* data, u64 size )
    {
        if ( offset + size > block.size() )
        {
            BUBBLE_ASSERT( false, "Uniform write past the end of the block" );
            return;
        }
        std::memcpy( block.data() + offset, data, size );
    };

    // A WGSL mat3x3 is three columns each padded to 16 bytes, so it cannot be
    // copied straight out of a glm::mat3.
    const auto writeMat3 = [&]( u32 offset, const mat3& value )
    {
        for ( i32 column = 0; column < 3; column++ )
            write( offset + (u32)column * 16, value_ptr( value[column] ), sizeof( vec3 ) );
    };

    // WGSL has no host shareable bool, so a shader spells one u32.
    const auto writeBool = [&]( u32 offset, bool value )
    {
        const u32 asUint = value ? 1u : 0u;
        write( offset, &asUint, sizeof( asUint ) );
    };

    // Seed with each uniform's declared default before applying the table.
    //
    // A uniform the table has no entry for has to end up at its default, not at
    // zero. Under OpenGL that happened by itself: nothing was written, so the
    // program kept the value its GLSL initializer gave it. Writing into a buffer
    // there is no such fallback, and a defaulted uColor would come out black.
    for ( const auto& [name, type] : shader.mUniformDescriptors )
    {
        if ( type == GLSLDataType::Texture2D )
            continue;

        const auto offsetIt = shader.mUniformOffsets.find( name );
        const auto defaultIt = shader.mUniformDefaults.find( name );
        if ( offsetIt == shader.mUniformOffsets.end() or
             defaultIt == shader.mUniformDefaults.end() )
            continue;

        const u32 offset = offsetIt->second;
        const UniformDefault& value = defaultIt->second;

        switch ( type )
        {
            case GLSLDataType::Float:
            case GLSLDataType::Float2:
            case GLSLDataType::Float3:
            case GLSLDataType::Float4:
                write( offset, value.mFloats.data(),
                       sizeof( f32 ) * GLSLDataComponentCount( type ) );
                break;
            case GLSLDataType::Mat3:
                // Stored row after row in the default, but laid out as three
                // padded columns in the block.
                for ( u32 column = 0; column < 3; column++ )
                    write( offset + column * 16, value.mFloats.data() + column * 3,
                           sizeof( f32 ) * 3 );
                break;
            case GLSLDataType::Mat4:
                write( offset, value.mFloats.data(), sizeof( f32 ) * 16 );
                break;
            case GLSLDataType::Int:
            case GLSLDataType::Int2:
            case GLSLDataType::Int3:
            case GLSLDataType::Int4:
            case GLSLDataType::UInt:
            case GLSLDataType::Bool:
                write( offset, value.mInts.data(),
                       sizeof( i32 ) * GLSLDataComponentCount( type ) );
                break;
            default:
                break;
        }
    }

    for ( const auto& [name, type] : shader.mUniformDescriptors )
    {
        // A sampler is not part of the block. User textures still need their own
        // bindings, which group 3 does not carry yet.
        if ( type == GLSLDataType::Texture2D )
            continue;

        const auto offsetIt = shader.mUniformOffsets.find( name );
        if ( offsetIt == shader.mUniformOffsets.end() )
            continue;
        const u32 offset = offsetIt->second;

        sol::object val = uniforms[name];
        if ( !val.valid() || val.is<sol::nil_t>() )
            continue;

        switch ( type )
        {
            case GLSLDataType::Float:
                if ( val.is<float>() )
                {
                    const f32 value = val.as<float>();
                    write( offset, &value, sizeof( value ) );
                }
                break;
            case GLSLDataType::Float2:
                if ( val.is<vec2>() )
                {
                    const vec2 value = val.as<vec2>();
                    write( offset, value_ptr( value ), sizeof( value ) );
                }
                break;
            case GLSLDataType::Float3:
                if ( val.is<vec3>() )
                {
                    const vec3 value = val.as<vec3>();
                    write( offset, value_ptr( value ), sizeof( value ) );
                }
                break;
            case GLSLDataType::Float4:
                if ( val.is<vec4>() )
                {
                    const vec4 value = val.as<vec4>();
                    write( offset, value_ptr( value ), sizeof( value ) );
                }
                break;
            case GLSLDataType::Mat3:
                if ( val.is<mat3>() )
                    writeMat3( offset, val.as<mat3>() );
                break;
            case GLSLDataType::Mat4:
                if ( val.is<mat4>() )
                {
                    const mat4 value = val.as<mat4>();
                    write( offset, value_ptr( value ), sizeof( value ) );
                }
                break;
            case GLSLDataType::Int:
                if ( val.is<int>() )
                {
                    const i32 value = val.as<int>();
                    write( offset, &value, sizeof( value ) );
                }
                break;
            case GLSLDataType::UInt:
                if ( val.is<int>() )
                {
                    const u32 value = (u32)val.as<int>();
                    write( offset, &value, sizeof( value ) );
                }
                break;
            case GLSLDataType::Bool:
                // RebuildUniforms defaults a bool uniform to Lua `false`, and
                // the inspector edits it as a checkbox - neither of which is a
                // Lua number, so an is<int>() test would never match and bool
                // uniforms would never reach the shader at all.
                if ( val.is<bool>() )
                    writeBool( offset, val.as<bool>() );
                break;
            case GLSLDataType::Int2:
                if ( val.is<ivec2>() )
                {
                    const ivec2 value = val.as<ivec2>();
                    write( offset, value_ptr( value ), sizeof( value ) );
                }
                break;
            case GLSLDataType::Int3:
                if ( val.is<ivec3>() )
                {
                    const ivec3 value = val.as<ivec3>();
                    write( offset, value_ptr( value ), sizeof( value ) );
                }
                break;
            case GLSLDataType::Int4:
                if ( val.is<ivec4>() )
                {
                    const ivec4 value = val.as<ivec4>();
                    write( offset, value_ptr( value ), sizeof( value ) );
                }
                break;
            default:
                break;
        }
    }
}

} // namespace bubble

