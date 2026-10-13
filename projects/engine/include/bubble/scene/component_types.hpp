#pragma once
#include "bubble/reflection/reflection.hpp"
#include "bubble/scene/scene.hpp"

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
class ComponentTypes
{
public:
    struct Type
    {
        Name mName;
        MetaType mType;
        // What a file holds for one: how JSON is read back.
        ValueType mValueType;
        // The entity's component by reference; empty when it has none.
        function<MetaAny( const Scene&, Entity )> mGet;
        // Gives the entity the component `made` holds, moving it.
        function<void( Scene&, Entity, MetaAny& made )> mAdd;
    };

    explicit ComponentTypes( Reflection& reflection ) : mReflection( reflection ) {}

    // A type not described yet, or added twice, is a logic_error.
    template <typename T>
    void Add()
    {
        const Reflection& reflection = mReflection;
        AddType(
            reflection.TypeOf<T>(),
            [&reflection]( const Scene& scene, Entity entity ) {
                const auto componentMaybe = scene.TryGet<T>( entity );
                return componentMaybe ? reflection.Ref( *componentMaybe ) : MetaAny();
            },
            []( Scene& scene, Entity entity, MetaAny& made ) {
                scene.Add<T>( entity, std::move( made.cast<T&>() ) );
            } );
    }

    // In the order they were added: the order a file lists them in.
    span<const Type> All() const { return mTypes; }
    OptRef<const Type> TryFind( Name name ) const;
    // For messages: "light, health".
    string Names() const;

    Reflection& GetReflection() const { return mReflection; }

private:
    void AddType( const MetaType& type,
                  function<MetaAny( const Scene&, Entity )> get,
                  function<void( Scene&, Entity, MetaAny& )> add );

    Reflection& mReflection;
    vector<Type> mTypes;
};
}
