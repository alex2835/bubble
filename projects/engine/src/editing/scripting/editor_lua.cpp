#include "engine/pch/pch.hpp"
#include "engine/editing/scripting/editor_lua.hpp"
#include "engine/editing/history.hpp"
#include "engine/editing/selection.hpp"
#include "engine/project/project.hpp"
#include "engine/serialization/types_serialization.hpp"
#include "glm_lua_bindings.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>
#include <fstream>
#include <sstream>
#include "engine/scene/components/tag_component.hpp"

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
    LogInfo( "[editor lua] {}", line );
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

    // print goes to the console, not stdout
    lua.set_function( "print", [this]( sol::variadic_args args )
    {
        string line;
        for ( const auto& arg : args )
        {
            if ( not line.empty() )
                line += '\t';
            line += ( *mLua )["tostring"]( arg.get<sol::object>() ).get<string>();
        }
        Print( std::move( line ) );
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
    editor.set_function( "select_node", [this]( u64 id )
    {
        auto node = FindNodeById( id, mCtx.mProject.mLevel.mTreeRoot );
        if ( not node )
            throw std::runtime_error( std::format( "select_node: no node with id {}", id ) );
        mCtx.mSelection.SelectTreeNode( node, mCtx.mProject.mLevel.mScene );
    } );
    editor.set_function( "select", [this]( sol::variadic_args ids )
    {
        Scene& scene = mCtx.mProject.mLevel.mScene;
        mCtx.mSelection.Clear();
        for ( const auto& arg : ids )
        {
            const Entity entity = scene.GetEntityById( arg.get<u64>() );
            if ( not scene.HasEntity( entity ) )
                throw std::runtime_error( std::format( "select: no entity {}", arg.get<u64>() ) );
            mCtx.mSelection.AddEntity( entity, scene );
        }
    } );
    editor.set_function( "deselect", [this]() { mCtx.mSelection.Clear(); } );

    /// The document, read-only
    editor.set_function( "tree", [this]()
    {
        // { id, type, name | entity, children = { ... } }
        std::function<sol::table( const Ref<ProjectTreeNode>& )> describe;
        describe = [&]( const Ref<ProjectTreeNode>& node )
        {
            sol::table t = mLua->create_table();
            t["id"] = node->ID();
            t["type"] = string( magic_enum::enum_name( node->Type() ) );
            if ( node->IsEntity() )
                t["entity"] = (u64)node->AsEntity();
            else
                t["name"] = std::get<string>( node->State() );
            sol::table children = mLua->create_table();
            int i = 1;
            for ( const auto& child : node->mChildren )
                children[i++] = describe( child );
            t["children"] = children;
            return t;
        };
        return describe( mCtx.mProject.mLevel.mTreeRoot );
    } );
    editor.set_function( "entities_by_tag", [this]( const string& tag )
    {
        sol::table ids = mLua->create_table();
        int i = 1;
        mCtx.mProject.mLevel.mScene.ForEach<TagComponent>( [&]( Entity entity, const TagComponent& t )
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
