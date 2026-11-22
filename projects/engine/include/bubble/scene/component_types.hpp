#pragma once
#include "bubble/reflection/reflection.hpp"
#include "bubble/scene/scene.hpp"
#include "bubble/scene/scene_snapshot.hpp"

namespace bubble
{
// The components a scene file holds, under the names reflection describes
// them by: what TrySaveScene writes and TryLoadScene knows how to make. A type is
// described first, then added here:
//
//     reflection.Type<Light>( "light" ).Field<&Light::mBrightness>( "brightness" );
//     componentTypes.Add<Light>();
//
// A component of a type not added here stays out of files.
//
// Beside them are the engine's own components for its bookkeeping - which
// prefab an instance is of - kept by undo and by copies of a subtree in
// memory, never written as components: a file says them its own way.
class ComponentTypes
{
public:
    struct Type
    {
        Name mName;
        // What it is about, for the editor to group types by: "physics",
        // "lighting", "ui". Snake_case, as everything a user sees.
        string mCategory;
        MetaType mType;
        // What a file holds for one: how JSON is read back.
        ValueType mValueType;
        // The entity's component by reference; empty when it has none.
        // Copying the MetaAny copies the component.
        function<MetaAny( const Scene&, Entity )> mGet;
        // The same, to be written through.
        function<MetaAny( Scene&, Entity )> mGetWritable;
        // Gives the entity the component `made` holds, moving it.
        function<void( Scene&, Entity, MetaAny& made )> mAdd;
        function<void( Scene&, Entity )> mRemove;
        // Every component of the type at once, for a snapshot: CopyComponents
        // and AddComponents.
        function<entt::any( const Scene& )> mCopyAll;
        function<void( Scene&, const entt::any& copies )> mAddAll;
        // The id EnTT keeps the type's components under: what a world over a
        // scene it does not own keeps when it goes (Scene::RemoveComponentsExcept).
        entt::id_type mStorage = 0;
    };

    // With the engine's own internal types: PrefabInstance, PrefabLink.
    explicit ComponentTypes( Reflection& reflection );

    // Under `category`, for the editor to group it with its like. A type
    // not described yet, or added twice, is a logic_error.
    template <typename T>
    void Add( string_view category = cOther )
    {
        // A component is data: one added in the editor starts as its
        // defaults, and undo and copies keep one by copying it.
        static_assert( default_initializable<T> and copy_constructible<T>,
                       "a component is made with its defaults and copied" );
        Type made = Make<T>();
        made.mCategory = category;
        AddType( mReflection.TypeOf<T>(), std::move( made ) );
    }

    // The category of a type added with none.
    static constexpr string_view cOther = "other";

    // In the order they were added: the order a file lists them in.
    span<const Type> All() const { return mTypes; }
    OptRef<const Type> TryFind( Name name ) const;
    // The engine's own; no reflection, no value type, never in a file.
    span<const Type> Internal() const { return mInternal; }
    // Among both: what a copy of a subtree in memory looks up.
    OptRef<const Type> TryFindAny( Name name ) const;
    // For messages: "light, health".
    string Names() const;

    // Sets the field at `path` of the entity's component of `type`, as
    // Reflection::TrySetField does, and tells the scene's field listeners -
    // a world's modules hear which field of which entity changed. An entity
    // without the component is an error saying so.
    expected<void, string> TrySetField( Scene& scene,
                                        Entity entity,
                                        const Type& type,
                                        const FieldPath& path,
                                        MetaAny value ) const;

    Reflection& GetReflection() const { return mReflection; }

private:
    // The functions of a Type, for T; the rest is filled in by who adds it.
    template <typename T>
    Type Make() const
    {
        Reflection& reflection = mReflection;
        Type type;
        type.mGet = [&reflection]( const Scene& scene, Entity entity ) {
            const auto componentMaybe = scene.TryGet<T>( entity );
            return componentMaybe ? reflection.Ref( *componentMaybe ) : MetaAny();
        };
        type.mGetWritable = [&reflection]( Scene& scene, Entity entity ) {
            const auto componentMaybe = scene.TryGet<T>( entity );
            return componentMaybe ? reflection.Ref( *componentMaybe ) : MetaAny();
        };
        type.mAdd = []( Scene& scene, Entity entity, MetaAny& made ) {
            scene.Add<T>( entity, std::move( made.cast<T&>() ) );
        };
        type.mRemove = []( Scene& scene, Entity entity ) { scene.Remove<T>( entity ); };
        type.mCopyAll = &CopyComponents<T>;
        type.mAddAll = &AddComponents<T>;
        type.mStorage = entt::type_hash<T>::value();
        return type;
    }

    // Fills in the name, the type and the value type of `made`.
    void AddType( const MetaType& type, Type made );

    template <typename T>
    void AddInternal( string_view name );

    Reflection& mReflection;
    vector<Type> mTypes;
    vector<Type> mInternal;
};
}
