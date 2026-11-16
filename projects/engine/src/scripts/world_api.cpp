#include "world_api.hpp"
#include "bubble/assets/asset_registry.hpp"
#include "bubble/core/asset_path.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scripts/script_values.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include "bubble/world/world.hpp"
#include <cstring>

namespace bubble
{
namespace
{
// An entity a script asks something of: there, or the script's mistake.
const Scene& Living( World& world, Entity entity )
{
    const Scene& scene = world.GetScene();
    if ( entity == Entity::Null )
        throw LuaError( "an empty entity: an entity() prop the scene has not set" );
    if ( not scene.Alive( entity ) )
        throw LuaError( "the entity is gone" );
    return scene;
}

// A component as a script holds it: its entity. Each component has a Luau
// type of its own over it, so the type says which.
struct ComponentRef
{
    Entity mEntity;
};

using ComponentType = ComponentTypes::Type;

// "transform, script", for messages.
string ComponentsOf( World& world, const Scene& scene, Entity entity )
{
    vector<string> names;
    for ( const ComponentType& type : world.GetApp().GetComponentTypes().All() )
        if ( type.mGet( scene, entity ) )
            names.push_back( type.mName.ToString() );
    ranges::sort( names );
    string list;
    for ( const string& name : names )
        list += ( list.empty() ? "" : ", " ) + name;
    return list.empty() ? "none" : list;
}

const ComponentType& ComponentNamed( World& world, const string& name )
{
    const ComponentTypes& types = world.GetApp().GetComponentTypes();
    const auto nameMaybe = Name::TryFrom( name );
    const auto typeMaybe = nameMaybe ? types.TryFind( *nameMaybe ) : nullopt;
    if ( not typeMaybe )
        throw LuaError( format( "no component \"{}\" (components: {})", name, types.Names() ) );
    return *typeMaybe;
}

// The component's Luau type: a field for each field its description has
// for files - read as a script sees a prop, written through ComponentTypes
// so the world's modules hear of it.
LuaType& RegisterComponent( LuaState& lua, World& world, const ComponentType& type )
{
    LuaTypeBuilder<ComponentRef> builder( lua, type.mName.ToString() );
    Reflection& reflection = world.GetApp().GetReflection();
    reflection.ForEachField( type.mType, [&]( Name field, const MetaField& meta ) {
        const auto declared = ranges::find( type.mValueType.mFields, field, &ValueTypeField::mName );
        // A transient field is the owner's own state, rebuilt; not data.
        if ( declared == type.mValueType.mFields.end() )
            return;
        const string where = type.mName.ToString() + "." + field.ToString();
        const auto get = [&world, &lua, type, field, where]( ComponentRef& self ) -> LuaValue {
            const Scene& scene = world.GetScene();
            const MetaAny component = scene.Alive( self.mEntity ) ? type.mGet( scene, self.mEntity ) : MetaAny();
            if ( not component )
                throw LuaError( format( "{}: {} has no {} any more", where, Describe( scene, self.mEntity ),
                                        type.mName.ToString() ) );
            const Reflection& described = world.GetApp().GetReflection();
            auto valueMaybe = described.TryGet( component, field ).and_then( [&]( const MetaAny& value ) {
                return described.TryToValue( value );
            } );
            if ( not valueMaybe )
                throw LuaError( format( "{}: {}", where, valueMaybe.error() ) );
            auto luaMaybe = TryLuaFromValue( lua, *valueMaybe, scene, self.mEntity, where );
            if ( not luaMaybe )
                throw LuaError( luaMaybe.error() );
            return *luaMaybe;
        };
        if ( Reflection::InfoOf( meta ).Has( FieldInfo::ReadOnly ) )
        {
            builder.Property( field.ToString(), get );
            return;
        }
        builder.Property( field.ToString(), get,
                          [&world, &lua, type, path = *FieldPath::TryParse( field.ToString() ),
                           fieldType = declared->mType, where]( ComponentRef& self, const LuaValue& value ) {
                              Scene& scene = world.GetScene();
                              auto valueMaybe = TryValueFromLua( lua, value, fieldType, scene, where );
                              if ( not valueMaybe )
                                  throw LuaError( valueMaybe.error() );
                              const ComponentTypes& types = world.GetApp().GetComponentTypes();
                              auto setMaybe = types.TrySetField( scene, self.mEntity, type, path,
                                                                 MetaAny( std::move( *valueMaybe ) ) );
                              if ( not setMaybe )
                                  throw LuaError( format( "{}: {}", where, setMaybe.error() ) );
                          } );
    } );
    builder.ToString( [&world, type]( const ComponentRef& self ) {
        return format( "{} of {}", type.mName.ToString(), Describe( world.GetScene(), self.mEntity ) );
    } );
    return builder.Type();
}

opt<Entity> TryFind( World& world, Entity from, string_view path )
{
    const Scene& scene = Living( world, from );
    auto textMaybe = Text::TryFrom( path );
    if ( not textMaybe or path.empty() )
        return nullopt;
    const auto entityMaybe = TryResolve( scene, EntityPath{ std::move( *textMaybe ) }, from );
    return entityMaybe ? opt<Entity>( *entityMaybe ) : nullopt;
}

void RegisterEntity( LuaState& lua, World& world )
{
    // The components' types first: the entity's fields hand them out.
    vector<pair<ComponentType, LuaType*>> components;
    for ( const ComponentType& type : world.GetApp().GetComponentTypes().All() )
        components.emplace_back( type, &RegisterComponent( lua, world, type ) );

    LuaTypeBuilder<Entity> entity( lua, "entity" );
    for ( const auto& [type, luaType] : components )
        entity.Property( type.mName.ToString(), [&world, type, &componentType = *luaType]( Entity& self ) {
            const Scene& scene = Living( world, self );
            if ( not type.mGet( scene, self ) )
                throw LuaError( format( "{} has no {} (it has: {})", Describe( scene, self ), type.mName.ToString(),
                                        ComponentsOf( world, scene, self ) ) );
            return componentType.New( ComponentRef{ self } );
        } );
    entity
        .Method( "has",
                 [&world]( Entity& self, const string& component ) {
                     const ComponentType& type = ComponentNamed( world, component );
                     return static_cast<bool>( type.mGet( Living( world, self ), self ) );
                 } )
        .Method( "add",
                 [&world]( Entity& self, const string& component ) {
                     const ComponentType& type = ComponentNamed( world, component );
                     Scene& scene = world.GetScene();
                     (void)Living( world, self );
                     if ( type.mGet( scene, self ) )
                         throw LuaError( format( "{} has a {} already", Describe( scene, self ), component ) );
                     MetaAny made = type.mType.construct();
                     type.mAdd( scene, self, made );
                 } )
        .Method( "remove",
                 [&world]( Entity& self, const string& component ) {
                     const ComponentType& type = ComponentNamed( world, component );
                     (void)Living( world, self );
                     type.mRemove( world.GetScene(), self );
                 } )
        .Property( "name", [&world]( Entity& self ) { return NameOf( Living( world, self ), self ).String(); } )
        .Property( "path", [&world]( Entity& self ) { return PathOf( Living( world, self ), self ).mPath.String(); } )
        .Property( "parent",
                   [&world]( Entity& self ) {
                       const Entity parent = ParentOf( Living( world, self ), self );
                       return parent == Entity::Null ? opt<Entity>() : opt<Entity>( parent );
                   } )
        .Property( "alive", [&world]( Entity& self ) { return world.GetScene().Alive( self ); } )
        .Method( "child",
                 [&world]( Entity& self, const string& name ) {
                     return name.find( '/' ) == string::npos ? TryFind( world, self, name ) : nullopt;
                 } )
        .Method( "find", [&world]( Entity& self, const string& path ) { return TryFind( world, self, path ); } )
        .Method( "destroy",
                 [&world]( Entity& self ) {
                     Scene& scene = world.GetScene();
                     if ( self == scene.Root() )
                         throw LuaError( "the root goes with the world, not by destroy()" );
                     (void)Living( world, self );
                     scene.Destroy( self );
                 } )
        .ToString( [&world]( const Entity& self ) { return Describe( world.GetScene(), self ); } )
        // By its path: what lasts from one run of a level to the next. One no
        // longer there comes back empty.
        .Saved(
            [&world]( const Entity& self ) {
                const Scene& scene = world.GetScene();
                return scene.Alive( self ) ? PathOf( scene, self ).mPath.String() : string();
            },
            [&world]( string_view bytes ) -> expected<Entity, string> {
                auto textMaybe = Text::TryFrom( bytes );
                if ( not textMaybe )
                    return unexpected( std::move( textMaybe.error() ) );
                if ( bytes.empty() )
                    return Entity::Null;
                return TryResolve( world.GetScene(), EntityPath{ std::move( *textMaybe ) } ).value_or( Entity::Null );
            } );
    // For props: `door = entity()` is a link the scene sets.
    lua.Globals()["entity"] = []() { return Entity::Null; };
}

void RegisterColor( LuaState& lua )
{
    // Read-only, as a vector is: a color is a value, not a place to write.
    LuaTypeBuilder<Color>( lua, "color" )
        .ReadOnly( "r", &Color::mR )
        .ReadOnly( "g", &Color::mG )
        .ReadOnly( "b", &Color::mB )
        .ReadOnly( "a", &Color::mA )
        .ToString( []( const Color& self ) {
            return format( "color( {}, {}, {}, {} )", WidenFloat( self.mR ), WidenFloat( self.mG ),
                           WidenFloat( self.mB ), WidenFloat( self.mA ) );
        } )
        .Saved(
            []( const Color& self ) {
                string bytes( sizeof( Color ), '\0' );
                std::memcpy( bytes.data(), &self, sizeof( Color ) );
                return bytes;
            },
            []( string_view bytes ) -> expected<Color, string> {
                if ( bytes.size() != sizeof( Color ) )
                    return unexpected( "damaged"s );
                Color color;
                std::memcpy( &color, bytes.data(), sizeof( Color ) );
                return color;
            } );
    lua.Globals()["color"] = []( f32 r, f32 g, f32 b, opt<f32> aMaybe ) {
        return Color{ .mR = r, .mG = g, .mB = b, .mA = aMaybe.value_or( 1.0f ) };
    };
}

void RegisterAsset( LuaState& lua, const AssetRegistry& assets )
{
    LuaTypeBuilder<AssetId>( lua, "asset" )
        .Property( "path",
                   [&assets]( AssetId& self ) {
                       const auto pathMaybe = assets.Catalog().TryPathOf( self );
                       return pathMaybe ? pathMaybe->String() : string();
                   } )
        .ToString( [&assets]( const AssetId& self ) { return self ? assets.Catalog().Describe( self ) : "no asset"s; } )
        .Saved( []( const AssetId& self ) { return self ? self.ToString() : string(); },
                []( string_view bytes ) -> expected<AssetId, string> {
                    if ( bytes.empty() )
                        return AssetId{};
                    const auto idMaybe = AssetId::TryParse( bytes );
                    if ( not idMaybe )
                        return unexpected( format( "'{}' is not an asset id", bytes ) );
                    return *idMaybe;
                } );
    // By path, as require names a file; nothing for a prop no one set yet.
    lua.Globals()["asset"] = [&assets]( opt<string> pathMaybe ) {
        if ( not pathMaybe )
            return AssetId{};
        auto validMaybe = AssetPath::TryFrom( *pathMaybe );
        if ( not validMaybe )
            throw LuaError( format( "asset( \"{}\" ): {}", *pathMaybe, validMaybe.error() ) );
        const auto idMaybe = assets.Catalog().TryIdOf( *validMaybe );
        if ( not idMaybe )
            throw LuaError( format( "asset( \"{}\" ): no such file in the project", *pathMaybe ) );
        return *idMaybe;
    };
}

// The world's Random instead of Luau's own: the same seed plays the same
// game, and a snapshot takes the numbers to come with it.
struct ServiceMethod
{
    MetaType mService;
    Name mMethod;
    MetaMethod mMeta;
    string mWhere;
};

// Each service described as one, a table of its methods by the type's
// name. The service is found when called: the module that adds it may come
// after the scripts.
void RegisterServices( LuaState& lua, World& world )
{
    const Reflection& reflection = world.GetApp().GetReflection();
    for ( const MetaType& service : reflection.Services() )
    {
        const string serviceName = reflection.NameOf( service );
        LuaTable table = lua.NewTable();
        reflection.ForEachMethod( service, [&]( Name method, const MetaMethod& meta ) {
            // Held by a Ref: a bound function moves without throwing, and
            // reflection's handles do not promise to.
            const auto call = CreateRef<const ServiceMethod>(
                ServiceMethod{ service, method, meta, serviceName + "." + method.ToString() } );
            table[method.ToString()] = [&world, &lua, call]( const LuaRest& rest ) {
                const MetaType& service = call->mService;
                const MetaMethod& meta = call->mMeta;
                const Name method = call->mMethod;
                const string& where = call->mWhere;
                const auto anyMaybe = world.GetServices().TryGetAny( service.info().hash() );
                if ( not anyMaybe )
                    throw LuaError( format( "{}: the world has no {}: the module that adds it is missing", where,
                                            world.GetApp().GetReflection().NameOf( service ) ) );
                if ( rest.mValues.size() != meta.arity() )
                    throw LuaError(
                        format( "{} takes {} arguments, got {}", where, meta.arity(), rest.mValues.size() ) );
                const Reflection& described = world.GetApp().GetReflection();
                vector<MetaAny> args;
                for ( size_t i = 0; i < meta.arity(); ++i )
                {
                    const string argument = format( "{}: argument {}", where, i + 1 );
                    auto argMaybe =
                        described.TryValueTypeOf( meta.arg( i ) )
                            .and_then( [&]( const ValueType& type ) {
                                return TryValueFromLua( lua, rest.mValues[i], type, world.GetScene(), argument );
                            } )
                            .and_then(
                                [&]( const Value& value ) { return described.TryFromValue( value, meta.arg( i ) ); } );
                    if ( not argMaybe )
                        throw LuaError( argMaybe.error() );
                    args.push_back( std::move( *argMaybe ) );
                }
                MetaAny object = service.from_void( anyMaybe->data() );
                auto resultMaybe = world.GetApp().GetReflection().TryCall( object, method, args );
                if ( not resultMaybe )
                    throw LuaError( resultMaybe.error() );
                if ( meta.ret().info() == entt::type_id<void>() )
                    return LuaValue();
                auto valueMaybe = described.TryToValue( *resultMaybe );
                if ( not valueMaybe )
                    throw LuaError( format( "{}: {}", where, valueMaybe.error() ) );
                auto luaMaybe = TryLuaFromValue( lua, *valueMaybe, world.GetScene(), Entity::Null, where );
                if ( not luaMaybe )
                    throw LuaError( luaMaybe.error() );
                return *luaMaybe;
            };
        } );
        lua.Globals()[serviceName] = table;
    }
}

// input.down( "jump" ): what the fixed step running sees, else the frame.
// A world nobody plays has no actions, and every one is at rest.
void RegisterInput( LuaState& lua, World& world )
{
    const auto state = [&world]( const string& action ) {
        const Actions& actions = world.GetActions();
        if ( not actions.Mapped() )
            return ActionState{};
        const auto nameMaybe = Name::TryFrom( action );
        if ( not nameMaybe )
            throw LuaError( format( "input: {}", nameMaybe.error() ) );
        auto stateMaybe = actions.TryState( *nameMaybe );
        if ( not stateMaybe )
            throw LuaError( format( "input: {}", stateMaybe.error() ) );
        return *stateMaybe;
    };
    LuaTable input = lua.NewTable();
    input["down"] = [state]( const string& action ) { return state( action ).mDown; };
    input["pressed"] = [state]( const string& action ) { return state( action ).mPressed; };
    input["released"] = [state]( const string& action ) { return state( action ).mReleased; };
    input["axis"] = [state]( const string& action ) { return static_cast<f64>( state( action ).mValue ); };
    lua.Globals()["input"] = input;
}

void RegisterRandom( LuaState& lua, World& world )
{
    LuaTable math( lua.Globals().RawGet( "math" ) );
    math["random"] = [&world]( opt<i64> firstMaybe, opt<i64> secondMaybe ) -> f64 {
        Random& random = world.GetRandom();
        if ( not firstMaybe )
            return random.Float();
        const i64 low = secondMaybe ? *firstMaybe : 1;
        const i64 high = secondMaybe ? *secondMaybe : *firstMaybe;
        if ( low > high )
            throw LuaError( format( "math.random: the interval [{}, {}] is empty", low, high ) );
        const u64 span = static_cast<u64>( high - low ) + 1;
        if ( span > 0xFFFFFFFFull )
            throw LuaError( format( "math.random: the interval [{}, {}] is wider than 2^32 - 1", low, high ) );
        return static_cast<f64>( low + static_cast<i64>( random.Below( static_cast<u32>( span ) ) ) );
    };
    math["randomseed"] = []( const LuaRest& ) {
        throw LuaError( "math.randomseed: a world's numbers come from its seed, the same in every replay" );
    };
}
}

void RegisterWorldApi( LuaState& lua, World& world, const AssetRegistry& assets )
{
    RegisterEntity( lua, world );
    RegisterColor( lua );
    RegisterAsset( lua, assets );
    RegisterServices( lua, world );
    RegisterInput( lua, world );
    RegisterRandom( lua, world );
}
}
