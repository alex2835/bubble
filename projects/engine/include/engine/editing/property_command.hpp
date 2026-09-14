#pragma once
#include "engine/editing/command.hpp"
#include "engine/scene/scene.hpp"
#include <functional>

namespace bubble
{
// One property of one component set from `old` to `new`. The property is
// addressed by (scene, entity, apply) rather than by a pointer, since the
// component moves inside its pool; `apply` is how a value lands on it - a
// member assignment, a setter, or a whole-component copy for the cases where
// changing one thing rebuilds the rest (a rigid body's shape).
//
// Made by the widgets in undoable_widgets.hpp, rarely by hand.
template <typename Component, typename T>
class SetPropertyCommand : public ICommand
{
public:
    using Apply = std::function<void( Component&, const T& )>;

    SetPropertyCommand( Scene& scene, Entity entity, string name, T oldValue, T newValue, Apply apply )
        : mScene( scene ),
          mEntity( entity ),
          mName( std::move( name ) ),
          mOld( std::move( oldValue ) ),
          mNew( std::move( newValue ) ),
          mApply( std::move( apply ) )
    {
    }

    string_view Name() const override { return mName; }
    void Execute() override { Set( mNew ); }
    void Undo() override { Set( mOld ); }

private:
    void Set( const T& value )
    {
        // The entity can be gone by the time this is undone across a deleted
        // and restored entity; the restore puts the value back on its own.
        if ( mScene.HasComponent<Component>( mEntity ) )
            mApply( mScene.GetComponent<Component>( mEntity ), value );
    }

    Scene& mScene;
    Entity mEntity;
    string mName;
    T mOld;
    T mNew;
    Apply mApply;
};

}
