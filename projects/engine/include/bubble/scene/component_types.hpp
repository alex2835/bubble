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
        // Copying the MetaAny copies the component.
        function<MetaAny( const Scene&, Entity )> mGet;
        // The same, to be written through.
        function<MetaAny( Scene&, Entity )> mGetWritable;
        // Gives the entity the component `made` holds, moving it.
        function<void( Scene&, Entity, MetaAny& made )> mAdd;
        function<void( Scene&, Entity )> mRemove;
    };

    explicit ComponentTypes( Reflection& reflection ) : mReflection( reflection ) {}

    // A type not described yet, or added twice, is a logic_error.
    template <typename T>
    void Add()
    {
        // A component is data: one added in the editor starts as its
        // defaults, and undo and copies keep one by copying it.
        static_assert( default_initializable<T> and copy_constructible<T>,
                       "a component is made with its defaults and copied" );
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
        AddType( reflection.TypeOf<T>(), std::move( type ) );
    }

    // In the order they were added: the order a file lists them in.
    span<const Type> All() const { return mTypes; }
    OptRef<const Type> TryFind( Name name ) const;
    // For messages: "light, health".
    string Names() const;

    Reflection& GetReflection() const { return mReflection; }

private:
    // Fills in the name, the type and the value type of `made`.
    void AddType( const MetaType& type, Type made );

    Reflection& mReflection;
    vector<Type> mTypes;
};
}
