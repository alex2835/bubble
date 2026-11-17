#include "bubble/scripts/script_declarations.hpp"
#include "bubble/scripts/script_api.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include "bubble/world/app.hpp"

// Kept in step with what src/scripts/world_api.cpp and the runtime bind:
// the descriptions give the components and services, the rest is written
// here as those files have it.
namespace bubble
{
namespace
{
string LuauType( const ValueType& type )
{
    switch ( type.mKind )
    {
        case ValueKind::Bool: return "boolean";
        case ValueKind::Number:
        case ValueKind::Integer: return "number";
        case ValueKind::Text:
        case ValueKind::Name: return "string";
        case ValueKind::Vec3: return "vector";
        case ValueKind::Color: return "color";
        case ValueKind::Asset: return "asset";
        case ValueKind::Entity: return "entity";
        case ValueKind::List: return "{ " + LuauType( *type.mElement ) + " }";
        case ValueKind::Record:
        {
            string fields;
            for ( const ValueTypeField& field : type.mFields )
                fields += ( fields.empty() ? "" : ", " ) + field.mName.ToString() + ": " + LuauType( field.mType );
            return "{ " + fields + " }";
        }
        // Whatever it holds: a script tells which before it uses it.
        case ValueKind::Any: return "unknown";
    }
    return "unknown";
}

// "from, to, color" as names, one a parameter; argN where none are given.
vector<string> ParameterNames( const MethodInfo& info, size_t count )
{
    vector<string> names;
    if ( info.mParameters )
    {
        string_view rest = info.mParameters;
        while ( not rest.empty() )
        {
            const size_t comma = rest.find( ',' );
            string_view name = rest.substr( 0, comma );
            while ( name.starts_with( ' ' ) )
                name.remove_prefix( 1 );
            names.emplace_back( name );
            rest = comma == string_view::npos ? string_view() : rest.substr( comma + 1 );
        }
    }
    for ( size_t i = names.size(); i < count; ++i )
        names.push_back( format( "arg{}", i + 1 ) );
    names.resize( count );
    return names;
}

void WriteComponents( App& app, const ScriptApi& api, string& out )
{
    const Reflection& reflection = app.GetReflection();
    for ( const ComponentTypes::Type& type : app.GetComponentTypes().All() )
    {
        out += format( "declare extern type {} with\n", type.mName.ToString() );
        reflection.ForEachField( type.mType, [&]( Name field, const MetaField& ) {
            const auto declared = ranges::find( type.mValueType.mFields, field, &ValueTypeField::mName );
            if ( declared != type.mValueType.mFields.end() )
                out += format( "    {}: {}\n", field.ToString(), LuauType( declared->mType ) );
        } );
        for ( const ScriptApi::Extension& extension : api.Extensions() )
            if ( extension.mComponent == type.mName )
                out += extension.mDeclaration;
        out += "end\n\n";
    }
}

void WriteEntity( App& app, string& out )
{
    out += "declare extern type entity with\n";
    out += "    name: string\n    path: string\n    parent: entity?\n    alive: boolean\n";
    for ( const ComponentTypes::Type& type : app.GetComponentTypes().All() )
        out += format( "    {}: {}\n", type.mName.ToString(), type.mName.ToString() );
    out += "    function has(self, component: component): boolean\n"
           "    function add(self, component: component): ()\n"
           "    function remove(self, component: component): ()\n"
           "    function child(self, name: string): entity?\n"
           "    function find(self, path: string): entity?\n"
           "    function destroy(self): ()\n"
           "end\n\n";
}

void WriteServices( App& app, string& out )
{
    const Reflection& reflection = app.GetReflection();
    for ( const MetaType& service : reflection.Services() )
    {
        out += format( "declare {}: {{\n", reflection.NameOf( service ) );
        reflection.ForEachMethod( service, [&]( Name method, const MetaMethod& meta ) {
            const vector<string> names = ParameterNames( Reflection::InfoOf( meta ), meta.arity() );
            string parameters;
            for ( size_t i = 0; i < meta.arity(); ++i )
            {
                const auto typeMaybe = reflection.TryValueTypeOf( meta.arg( i ) );
                parameters +=
                    format( "{}{}: {}", i == 0 ? "" : ", ", names[i], typeMaybe ? LuauType( *typeMaybe ) : "unknown"s );
            }
            string result = "()";
            if ( meta.ret().info() != entt::type_id<void>() )
            {
                const auto typeMaybe = reflection.TryValueTypeOf( meta.ret() );
                result = typeMaybe ? LuauType( *typeMaybe ) : "unknown";
            }
            if ( const char* tooltip = Reflection::InfoOf( meta ).mTooltip )
                out += format( "    -- {}\n", tooltip );
            out += format( "    {}: ({}) -> {},\n", method.ToString(), parameters, result );
        } );
        out += "}\n\n";
    }
}

// "jump" | "move_x"; string when the project declares none.
string Union( const vector<string>& labels )
{
    if ( labels.empty() )
        return "string";
    string joined;
    for ( const string& label : labels )
        joined += format( "{}\"{}\"", joined.empty() ? "" : " | ", label );
    return joined;
}
}

string WriteScriptDeclarations( App& app, const ScriptLabels& labels )
{
    string out = "-- What scripts see of the engine, written by it from its descriptions: do not edit.\n\n";

    vector<string> components;
    for ( const ComponentTypes::Type& type : app.GetComponentTypes().All() )
        components.push_back( type.mName.ToString() );
    ranges::sort( components );
    out += "type component = " + Union( components ) + "\n";
    out += "type action = " + Union( labels.mActions ) + "\n\n";

    out += "declare extern type color with\n    r: number\n    g: number\n    b: number\n    a: number\nend\n\n";
    out += "declare extern type asset with\n    path: string\nend\n\n";
    const ScriptApi api = CollectScriptApi( app );
    WriteComponents( app, api, out );
    WriteEntity( app, out );
    WriteServices( app, out );

    out += "declare input: {\n"
           "    down: (action: action) -> boolean,\n"
           "    pressed: (action: action) -> boolean,\n"
           "    released: (action: action) -> boolean,\n"
           "    axis: (action: action) -> number,\n"
           "}\n\n";

    out += "-- Props and their constructors. props and locals return their table, for self's type:\n"
           "--     local P = props { speed = 40 }\n"
           "--     type Self = typeof( P ) & { entity: entity }\n"
           "declare function props<T>(props: T): T\n"
           "declare function locals<T>(locals: T): T\n"
           "declare function prop<T>(default: T, hints: { min: number?, max: number?, tooltip: string? }?): T\n"
           "declare function color(r: number, g: number, b: number, a: number?): color\n"
           "declare function asset(path: string?): asset\n"
           "declare function entity(): entity\n\n";

    out += "-- What the game remembers between levels, and its save slots.\n"
           "declare progress: { [string]: unknown }\n"
           "declare save: {\n"
           "    write: (slot: string, data: { [string]: unknown }) -> (),\n"
           "    read: (slot: string) -> { [string]: unknown }?,\n"
           "    info: (slot: string) -> { version: string, level: asset, time: number }?,\n"
           "    list: () -> { string },\n"
           "}\n\n";

    out += "-- Coroutines and events of an instance.\n"
           "declare function start<A...>(fn: (A...) -> (), ...: A...): ()\n"
           "declare function wait(seconds: number?): ()\n"
           "declare function wait_until(condition: () -> boolean): ()\n"
           "declare function on<A...>(event: string, fn: (A...) -> ()): ()\n"
           "declare function emit(event: string, ...: unknown): ()\n";

    for ( const ScriptApi::Globals& globals : api.AllGlobals() )
        out += "\n" + globals.mDeclaration;
    if ( not api.Callbacks().empty() )
    {
        out += "\n-- Callbacks the engine calls besides on_start, on_fixed_update, on_update and on_destroy:\n";
        for ( const ScriptApi::Callback& callback : api.Callbacks() )
            out += "--     " + callback.mSignature + "\n";
    }
    return out;
}
}
