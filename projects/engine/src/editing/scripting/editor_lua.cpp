#include "engine/pch/pch.hpp"
#include "engine/editing/scripting/editor_lua.hpp"
#include "engine/editing/history.hpp"
#include "engine/editing/selection.hpp"
#include "engine/editing/commands/field_command.hpp"
#include "engine/scene/component_manager.hpp"
#include "engine/reflection/reflection.hpp"
#include "engine/project/project.hpp"
#include "engine/serialization/types_serialization.hpp"
#include "glm_lua_bindings.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>
#include <fstream>
#include <sstream>
#include <set>
#include "engine/scene/components/tag_component.hpp"
#include "engine/scene/components/folder_component.hpp"
#include "engine/scene/hierarchy.hpp"

namespace bubble
{
/// json <-> Lua

json LuaToJson( const sol::object& value )
{
    switch ( value.get_type() )
    {
        case sol::type::lua_nil:
            return nullptr;
        case sol::type::boolean:
            return value.as<bool>();
        case sol::type::string:
            return value.as<string>();
        case sol::type::number:
        {
            // Lua 5.4 keeps integers apart from floats; a whole number that
            // arrived as a float still names an id or a count.
            const double d = value.as<double>();
            if ( d == std::floor( d ) and std::abs( d ) < 9.0e15 )
                return static_cast<i64>( d );
            return d;
        }
        case sol::type::table:
        {
            const sol::table table = value.as<sol::table>();
            // An array when the keys are exactly 1..n.
            size_t count = 0;
            bool isArray = true;
            for ( const auto& [k, v] : table )
            {
                count++;
                if ( not k.is<int>() or k.as<int>() < 1 )
                    isArray = false;
            }
            if ( isArray and count > 0 )
            {
                json arr = json::array();
                for ( size_t i = 1; i <= count; i++ )
                {
                    const sol::object item = table[i];
                    if ( item.get_type() == sol::type::lua_nil )
                    {
                        isArray = false; // a hole: keys are not 1..n after all
                        break;
                    }
                    arr.push_back( LuaToJson( item ) );
                }
                if ( isArray )
                    return arr;
            }
            json obj = json::object();
            for ( const auto& [k, v] : table )
            {
                const string key = k.is<string>() ? k.as<string>() : std::format( "{}", k.as<double>() );
                obj[key] = LuaToJson( v );
            }
            return obj;
        }
        case sol::type::userdata:
        {
            // Through the glm to_json overloads: [x, y, z].
            if ( value.is<vec2>() ) { json j = value.as<vec2>(); return j; }
            if ( value.is<vec3>() ) { json j = value.as<vec3>(); return j; }
            if ( value.is<vec4>() ) { json j = value.as<vec4>(); return j; }
            throw std::runtime_error( "an operator argument may be a number, string, boolean, table or vec2/3/4" );
        }
        default:
            throw std::runtime_error( "an operator argument may be a number, string, boolean, table or vec2/3/4" );
    }
}

sol::object JsonToLua( sol::state_view lua, const json& value )
{
    switch ( value.type() )
    {
        case json::value_t::null:            return sol::make_object( lua, sol::lua_nil );
        case json::value_t::boolean:         return sol::make_object( lua, value.get<bool>() );
        case json::value_t::number_integer:  return sol::make_object( lua, value.get<i64>() );
        case json::value_t::number_unsigned: return sol::make_object( lua, value.get<u64>() );
        case json::value_t::number_float:    return sol::make_object( lua, value.get<double>() );
        case json::value_t::string:          return sol::make_object( lua, value.get<string>() );
        case json::value_t::array:
        {
            sol::table table = lua.create_table();
            int i = 1;
            for ( const auto& item : value )
                table[i++] = JsonToLua( lua, item );
            return table;
        }
        case json::value_t::object:
        {
            sol::table table = lua.create_table();
            for ( const auto& [k, v] : value.items() )
                table[k] = JsonToLua( lua, v );
            return table;
        }
        default:
            return sol::make_object( lua, sol::lua_nil );
    }
}

/// EditorLua

EditorLua::EditorLua( OperatorContext ctx, OperatorQueue& queue )
    : mCtx( ctx ),
      mQueue( queue ),
      mLua( CreateScope<sol::state>() )
{
    mLua->open_libraries( sol::lib::base, sol::lib::table, sol::lib::string, sol::lib::math );
    CreateVec2Bindings( *mLua );
    CreateVec3Bindings( *mLua );
    CreateVec4Bindings( *mLua );
    CreateMathFreeFunctionsBindings( *mLua );
    Bind();
}

EditorLua::~EditorLua() = default;



void EditorLua::Print( string line )
{
    LogMessage( line.starts_with( "error:" ) ? LogLevel::Error : LogLevel::Script, line );
    mLog.push_back( std::move( line ) );
}

namespace
{
json ArgsOf( const sol::optional<sol::object>& args )
{
    if ( not args or args->get_type() == sol::type::lua_nil )
        return json::object();
    json j = LuaToJson( *args );
    if ( not j.is_object() )
        throw std::runtime_error( "operator arguments must be a table of named fields" );
    return j;
}
}

void EditorLua::Bind()
{
    sol::state& lua = *mLua;
    sol::table editor = lua.create_named_table( "editor" );

    // print goes to the console, not stdout, and opens tables up on one
    // line; dump( value, depth ) lays them out over several.
    lua.set_function( "print", [this]( sol::variadic_args args )
    {
        string line;
        for ( const auto& arg : args )
        {
            if ( not line.empty() )
                line += '\t';
            const sol::object value = arg.get<sol::object>();
            line += value.get_type() == sol::type::string ? value.as<string>() : DescribeLuaValue( value, 3, false );
        }
        Print( std::move( line ) );
    } );
    lua.set_function( "dump", [this]( sol::object value, sol::optional<int> depth )
    {
        Print( DescribeLuaValue( value, depth.value_or( 4 ), true ) );
    } );

    /// Operators
    editor.set_function( "invoke", [this]( const string& name, sol::optional<sol::object> args )
    {
        return InvokeOperator( name, mCtx, ArgsOf( args ) );
    } );
    editor.set_function( "poll", [this]( const string& name, sol::optional<sol::object> args )
    {
        return PollOperator( name, mCtx, ArgsOf( args ) );
    } );
    // For anything that replaces the level or the project: runs after the
    // frame, like a menu item does.
    editor.set_function( "enqueue", [this]( const string& name, sol::optional<sol::object> args )
    {
        mQueue.Enqueue( name, ArgsOf( args ) );
    } );
    editor.set_function( "operators", [this]()
    {
        sol::table names = mLua->create_table();
        int i = 1;
        for ( const auto name : OperatorRegistry::Instance().Names() )
            names[i++] = string( name );
        return names;
    } );
    editor.set_function( "undo", [this]() { return InvokeOperator( "history.undo", mCtx ); } );
    editor.set_function( "redo", [this]() { return InvokeOperator( "history.redo", mCtx ); } );

    // editor.ops.group.verb{ ... } - resolved by name on access, so an
    // operator registered after this state was made is reachable too.
    lua.script( R"(
        editor.ops = setmetatable( {}, { __index = function( _, group )
            return setmetatable( {}, { __index = function( _, verb )
                local name = group .. "." .. verb
                return function( args ) return editor.invoke( name, args ) end
            end } )
        end } )
    )" );

    /// Selection
    editor.set_function( "selection", [this]()
    {
        sol::table ids = mLua->create_table();
        int i = 1;
        for ( const auto entity : mCtx.mSelection.GetEntities() )
            ids[i++] = (u64)entity;
        return ids;
    } );
    editor.set_function( "select", [this]( sol::variadic_args ids )
    {
        Scene& scene = mCtx.mLevel.mScene;
        mCtx.mSelection.Clear();
        for ( const auto& arg : ids )
        {
            const Entity entity = Entity::FromId( arg.get<u64>() );
            if ( not scene.HasEntity( entity ) )
                throw std::runtime_error( std::format( "select: no entity {}", arg.get<u64>() ) );
            mCtx.mSelection.AddEntity( entity, scene );
        }
    } );
    editor.set_function( "deselect", [this]() { mCtx.mSelection.Clear(); } );

    /// The document, read-only
    editor.set_function( "tree", [this]()
    {
        // { entity, name, folder, children = { ... } } from the root. The
        // entity ids are what `parent` and `entity` arguments take.
        Scene& scene = mCtx.mLevel.mScene;
        std::function<sol::table( Entity )> describe = [&]( Entity entity )
        {
            sol::table t = mLua->create_table();
            t["entity"] = (u64)entity;
            t["name"] = NameOf( scene, entity );
            t["folder"] = scene.HasComponent<FolderComponent>( entity );
            sol::table children = mLua->create_table();
            int i = 1;
            for ( const Entity child : ChildrenOf( scene, entity ) )
                children[i++] = describe( child );
            t["children"] = children;
            return t;
        };
        return describe( scene.Root() );
    } );
    // By a path of names from the root: editor.find( "ground/floor" ) -> id,
    // or an error saying what is missing where; editor.try_find gives nil
    // instead. Operators take the path itself too, wherever they take an id.
    editor.set_function( "find", [this]( const string& path )
    {
        Scene& scene = mCtx.mLevel.mScene;
        const Entity found = FindByPath( scene, scene.Root(), path );
        if ( found == Entity::Null )
            throw std::runtime_error( std::format( "find( \"{}\" ): {}", path, WhyPathFails( scene, scene.Root(), path ) ) );
        return (u64)found;
    } );
    editor.set_function( "try_find", [this]( const string& path ) -> sol::object
    {
        Scene& scene = mCtx.mLevel.mScene;
        const Entity found = FindByPath( scene, scene.Root(), path );
        if ( found == Entity::Null )
            return sol::make_object( *mLua, sol::lua_nil );
        return sol::make_object( *mLua, (u64)found );
    } );
    editor.set_function( "path", [this]( u64 id )
    {
        Scene& scene = mCtx.mLevel.mScene;
        return PathOf( scene, Entity::FromId( id ) );
    } );
    // A field's value: editor.get( entity, "Light.brightness" ), the entity
    // by id or by path from the root. What property.set would take back:
    // numbers, strings, booleans, enums by name, vectors and described types
    // as tables.
    editor.set_function( "get", [this]( sol::object entityArg, const string& path ) -> sol::object
    {
        Scene& scene = mCtx.mLevel.mScene;
        Entity entity = Entity::Null;
        if ( entityArg.is<string>() )
        {
            const string at = entityArg.as<string>();
            entity = FindByPath( scene, scene.Root(), at );
            if ( entity == Entity::Null )
                throw std::runtime_error( std::format( "get: nothing at '{}' - {}", at, WhyPathFails( scene, scene.Root(), at ) ) );
        }
        else
            entity = Entity::FromId( entityArg.as<u64>() );
        const ComponentField field = ParseComponentField( path );
        entt::meta_any component = RequireReflected( scene, entity, field.mComponentId );
        return JsonToLua( *mLua, ToJson( GetField( component, field.mPath ), ComponentManager::ContextOf( mCtx.mProject ) ) );
    } );
    editor.set_function( "entities_by_tag", [this]( const string& tag )
    {
        sol::table ids = mLua->create_table();
        int i = 1;
        mCtx.mLevel.mScene.ForEach<TagComponent>( [&]( Entity entity, const TagComponent& t )
        {
            if ( t.mName == tag )
                ids[i++] = (u64)entity;
        } );
        return ids;
    } );
    editor.set_function( "current_level", [this]() { return mCtx.mProject.CurrentLevel().generic_string(); } );
    editor.set_function( "levels", [this]()
    {
        sol::table files = mLua->create_table();
        int i = 1;
        for ( const auto& level : mCtx.mProject.Levels() )
            files[i++] = level.generic_string();
        return files;
    } );
    editor.set_function( "undo_name", [this]() { return string( mCtx.mHistory.NextUndoName() ); } );
}

string EditorLua::Run( string_view code, string_view chunkName )
{
    const string name = std::format( "={}", chunkName );
    const auto result = mLua->safe_script( code, sol::script_pass_on_error, name );
    if ( result.valid() )
        return {};
    const sol::error err = result;
    string message = err.what();
    Print( "error: " + message );
    return message;
}

string EditorLua::RunInteractive( string_view line )
{
    // As an expression first; a statement does not compile after `return`.
    const string asExpression = std::format( "return {}", line );
    sol::load_result chunk = mLua->load( asExpression, "=console" );
    if ( not chunk.valid() )
        return Run( line );

    sol::protected_function evaluate = chunk;
    const sol::protected_function_result result = evaluate();
    if ( not result.valid() )
    {
        const sol::error err = result;
        Print( std::format( "error: {}", err.what() ) );
        return err.what();
    }
    for ( int i = 0; i < result.return_count(); i++ )
    {
        const sol::object value = result.get<sol::object>( i );
        if ( value.get_type() != sol::type::lua_nil and value.get_type() != sol::type::none )
            Print( DescribeLuaValue( value, 4, true ) );
    }
    return {};
}

string EditorLua::RunFile( const path& file )
{
    std::ifstream stream( file );
    if ( not stream )
    {
        const string message = std::format( "cannot open {}", file.string() );
        Print( "error: " + message );
        return message;
    }
    std::stringstream code;
    code << stream.rdbuf();
    return Run( code.str(), file.filename().string() );
}

}
