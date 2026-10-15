#include "bubble/editing/edits.hpp"
#include "bubble/core/assert.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include <typeinfo>

namespace bubble
{
namespace
{
// A type the edit found, looked up again: a command keeps the name, as the
// list of types may grow and move.
const ComponentTypes::Type& TypeNamed( const ComponentTypes& componentTypes, Name component )
{
    const auto typeMaybe = componentTypes.TryFind( component );
    if ( not typeMaybe )
        throw logic_error( format( "{} is no component type any more", component.ToString() ) );
    return *typeMaybe;
}

// What `any` holds, owned: copying a MetaAny copies the object even when it
// holds a reference - but a temporary one would be moved, reference and all.
MetaAny Copy( const MetaAny& any )
{
    return MetaAny( any );
}

size_t IndexAmongSiblings( const Scene& scene, Entity entity )
{
    const span<const Entity> siblings = ChildrenOf( scene, ParentOf( scene, entity ) );
    return static_cast<size_t>( ranges::find( siblings, entity ) - siblings.begin() );
}

class SetFieldCommand final : public Command
{
public:
    SetFieldCommand( const ComponentTypes& componentTypes,
                     Entity entity,
                     Name component,
                     FieldPath path,
                     MetaAny value )
        : mComponentTypes( componentTypes ),
          mEntity( entity ),
          mComponent( component ),
          mPath( std::move( path ) ),
          mAfter( std::move( value ) )
    {
    }

    expected<void, string> TryDo( Scene& scene ) override
    {
        Reflection& reflection = mComponentTypes.GetReflection();
        MetaAny component = TypeNamed( mComponentTypes, mComponent ).mGetWritable( scene, mEntity );
        auto beforeMaybe = reflection.TryGetField( component, mPath );
        if ( not beforeMaybe )
            return unexpected( std::move( beforeMaybe.error() ) );
        mBefore = Copy( *beforeMaybe );
        if ( auto setMaybe = reflection.TrySetField( component, mPath, Copy( mAfter ) ); not setMaybe )
            return setMaybe;
        // What the field holds now, converted to its type: redo sets that.
        auto afterMaybe = reflection.TryGetField( component, mPath );
        if ( not afterMaybe )
            throw logic_error( format( "a field set and gone: {}", afterMaybe.error() ) );
        mAfter = Copy( *afterMaybe );
        return {};
    }

    void Undo( Scene& scene ) override { Set( scene, mBefore, "Undo" ); }
    void Redo( Scene& scene ) override { Set( scene, mAfter, "Redo" ); }

    bool Absorb( const Command& next ) override
    {
        if ( typeid( next ) != typeid( SetFieldCommand ) )
            return false;
        const auto& set = static_cast<const SetFieldCommand&>( next );
        if ( set.mEntity != mEntity or set.mComponent != mComponent or set.mPath != mPath )
            return false;
        mAfter = Copy( set.mAfter );
        return true;
    }

private:
    // A value of the field's own type, which it has taken before.
    void Set( Scene& scene, const MetaAny& value, string_view what )
    {
        MetaAny component = TypeNamed( mComponentTypes, mComponent ).mGetWritable( scene, mEntity );
        if ( auto setMaybe = mComponentTypes.GetReflection().TrySetField( component, mPath, Copy( value ) );
             not setMaybe )
            throw logic_error( format( "{} of a field set: {}", what, setMaybe.error() ) );
    }

    const ComponentTypes& mComponentTypes;
    Entity mEntity;
    Name mComponent;
    FieldPath mPath;
    MetaAny mBefore;
    // What is to be set; once done, what the field took.
    MetaAny mAfter;
};

class CreateEntityCommand final : public Command
{
public:
    CreateEntityCommand( Entity parent, Text name, size_t index )
        : mParent( parent ),
          mName( std::move( name ) ),
          mIndex( index )
    {
    }

    Entity Created() const { return mEntity; }

    expected<void, string> TryDo( Scene& scene ) override
    {
        mEntity = CreateChild( scene, mParent, mName, mIndex );
        mName = NameOf( scene, mEntity );
        return {};
    }

    // What was added to it since is undone before: it is as it was made.
    void Undo( Scene& scene ) override { scene.Destroy( mEntity ); }

    // The same entity, not a new one: the steps after this one name it.
    void Redo( Scene& scene ) override
    {
        scene.Revive( mEntity );
        const Text name = AddToTree( scene, mEntity, mParent, mName, mIndex );
        BUBBLE_ASSERT( name == mName, "a redo made the entity with another name" );
    }

private:
    Entity mParent;
    Text mName;
    size_t mIndex;
    Entity mEntity = Entity::Null;
};

class DestroyEntityCommand final : public Command
{
public:
    DestroyEntityCommand( const ComponentTypes& componentTypes, Entity entity )
        : mComponentTypes( componentTypes ),
          mEntity( entity )
    {
    }

    expected<void, string> TryDo( Scene& scene ) override
    {
        Take( scene, mEntity );
        scene.Destroy( mEntity );
        return {};
    }

    void Undo( Scene& scene ) override
    {
        // Parents before their children, each child last so far: the order
        // they were taken in puts every one back in its place.
        for ( Taken& taken : mTaken )
        {
            scene.Revive( taken.mEntity );
            const Text name = AddToTree( scene, taken.mEntity, taken.mParent, taken.mName, taken.mIndex );
            BUBBLE_ASSERT( name == taken.mName, "an undo brought an entity back with another name" );
            for ( auto& [component, kept] : taken.mComponents )
                TypeNamed( mComponentTypes, component ).mAdd( scene, taken.mEntity, kept );
        }
        mTaken.clear();
    }

private:
    // One entity of the subtree as it was.
    struct Taken
    {
        Entity mEntity;
        Entity mParent;
        size_t mIndex = 0;
        Text mName;
        vector<pair<Name, MetaAny>> mComponents;
    };

    void Take( const Scene& scene, Entity entity )
    {
        Taken taken{ .mEntity = entity,
                     .mParent = ParentOf( scene, entity ),
                     .mIndex = IndexAmongSiblings( scene, entity ),
                     .mName = NameOf( scene, entity ),
                     .mComponents = {} };
        for ( const ComponentTypes::Type& type : mComponentTypes.All() )
            if ( const MetaAny component = type.mGet( scene, entity ) )
                taken.mComponents.emplace_back( type.mName, Copy( component ) );
        mTaken.push_back( std::move( taken ) );
        for ( const Entity child : ChildrenOf( scene, entity ) )
            Take( scene, child );
    }

    const ComponentTypes& mComponentTypes;
    Entity mEntity;
    vector<Taken> mTaken;
};

class MoveEntityCommand final : public Command
{
public:
    MoveEntityCommand( Entity entity, Entity parent, size_t index )
        : mEntity( entity ),
          mParent( parent ),
          mIndex( index )
    {
    }

    expected<void, string> TryDo( Scene& scene ) override
    {
        mOldParent = ParentOf( scene, mEntity );
        mOldIndex = IndexAmongSiblings( scene, mEntity );
        mOldName = NameOf( scene, mEntity );
        return TrySetParent( scene, mEntity, mParent, mIndex );
    }

    void Undo( Scene& scene ) override
    {
        if ( auto movedMaybe = TrySetParent( scene, mEntity, mOldParent, mOldIndex ); not movedMaybe )
            throw logic_error( format( "Undo of a move: {}", movedMaybe.error() ) );
        // Its old name is free where it was: the scene is as it left it.
        Rename( scene, mEntity, mOldName );
    }

private:
    Entity mEntity;
    Entity mParent;
    size_t mIndex;
    Entity mOldParent;
    size_t mOldIndex = 0;
    Text mOldName;
};

class RenameEntityCommand final : public Command
{
public:
    RenameEntityCommand( Entity entity, Text name ) : mEntity( entity ), mWanted( std::move( name ) ) {}

    expected<void, string> TryDo( Scene& scene ) override
    {
        mOldName = NameOf( scene, mEntity );
        Rename( scene, mEntity, mWanted );
        return {};
    }

    void Undo( Scene& scene ) override { Rename( scene, mEntity, mOldName ); }

private:
    Entity mEntity;
    Text mWanted;
    Text mOldName;
};

class AddComponentCommand final : public Command
{
public:
    AddComponentCommand( const ComponentTypes& componentTypes, Entity entity, Name component )
        : mComponentTypes( componentTypes ),
          mEntity( entity ),
          mComponent( component )
    {
    }

    expected<void, string> TryDo( Scene& scene ) override
    {
        const ComponentTypes::Type& type = TypeNamed( mComponentTypes, mComponent );
        MetaAny made = type.mType.construct();
        type.mAdd( scene, mEntity, made );
        return {};
    }

    void Undo( Scene& scene ) override { TypeNamed( mComponentTypes, mComponent ).mRemove( scene, mEntity ); }

private:
    const ComponentTypes& mComponentTypes;
    Entity mEntity;
    Name mComponent;
};

class RemoveComponentCommand final : public Command
{
public:
    RemoveComponentCommand( const ComponentTypes& componentTypes, Entity entity, Name component )
        : mComponentTypes( componentTypes ),
          mEntity( entity ),
          mComponent( component )
    {
    }

    expected<void, string> TryDo( Scene& scene ) override
    {
        const ComponentTypes::Type& type = TypeNamed( mComponentTypes, mComponent );
        mKept = Copy( type.mGet( scene, mEntity ) );
        type.mRemove( scene, mEntity );
        return {};
    }

    void Undo( Scene& scene ) override
    {
        TypeNamed( mComponentTypes, mComponent ).mAdd( scene, mEntity, mKept );
        mKept = MetaAny();
    }

private:
    const ComponentTypes& mComponentTypes;
    Entity mEntity;
    Name mComponent;
    MetaAny mKept;
};

expected<void, string> TryCheckInTree( const Scene& scene, Entity entity )
{
    if ( not InTree( scene, entity ) )
        return unexpected( format( "not an entity of the document: {}", Describe( scene, entity ) ) );
    return {};
}

// The entity is the document's, and `component` a type it can have.
expected<void, string> TryCheckComponentType( const Document& document, Entity entity, Name component )
{
    const Scene& scene = document.GetScene();
    if ( auto inTreeMaybe = TryCheckInTree( scene, entity ); not inTreeMaybe )
        return inTreeMaybe;
    const ComponentTypes& componentTypes = document.GetComponentTypes();
    if ( not componentTypes.TryFind( component ) )
        return unexpected( format( "{}: {} is not a component (components: {})", Describe( scene, entity ),
                                   component.ToString(), componentTypes.Names() ) );
    return {};
}

bool Has( const Document& document, Entity entity, Name component )
{
    return static_cast<bool>(
        TypeNamed( document.GetComponentTypes(), component ).mGet( document.GetScene(), entity ) );
}

expected<void, string> TryCheckHas( const Document& document, Entity entity, Name component )
{
    if ( auto typeMaybe = TryCheckComponentType( document, entity, component ); not typeMaybe )
        return typeMaybe;
    if ( not Has( document, entity, component ) )
        return unexpected( format( "{} has no {}", Describe( document.GetScene(), entity ), component.ToString() ) );
    return {};
}

expected<void, string> TryCheckHasNot( const Document& document, Entity entity, Name component )
{
    if ( auto typeMaybe = TryCheckComponentType( document, entity, component ); not typeMaybe )
        return typeMaybe;
    if ( Has( document, entity, component ) )
        return unexpected(
            format( "{} has a {} already", Describe( document.GetScene(), entity ), component.ToString() ) );
    return {};
}

expected<void, string> TryDo( Document& document, Scope<Command> command, string label )
{
    return document.GetHistory().TryDo( std::move( command ), std::move( label ) );
}
}

expected<void, string> TrySetField( Document& document,
                                    Entity entity,
                                    Name component,
                                    const FieldPath& path,
                                    MetaAny value )
{
    if ( auto checkedMaybe = TryCheckHas( document, entity, component ); not checkedMaybe )
        return checkedMaybe;
    auto doneMaybe = TryDo(
        document,
        CreateScope<SetFieldCommand>( document.GetComponentTypes(), entity, component, path, std::move( value ) ),
        format( "set {}.{}", component.ToString(), path.ToString() ) );
    if ( not doneMaybe )
        return unexpected( format( "{}: {}", Describe( document.GetScene(), entity ), doneMaybe.error() ) );
    return {};
}

expected<Entity, string> TryCreateEntity( Document& document, Entity parent, const Text& name, size_t index )
{
    if ( auto inTreeMaybe = TryCheckInTree( document.GetScene(), parent ); not inTreeMaybe )
        return unexpected( std::move( inTreeMaybe.error() ) );
    auto command = CreateScope<CreateEntityCommand>( parent, name, index );
    const CreateEntityCommand& created = *command;
    if ( auto doneMaybe = TryDo( document, std::move( command ), format( "create {}", name.View() ) ); not doneMaybe )
        return unexpected( std::move( doneMaybe.error() ) );
    // Kept by the history, which does not drop a step it has just taken.
    return created.Created();
}

expected<void, string> TryDestroyEntity( Document& document, Entity entity )
{
    const Scene& scene = document.GetScene();
    if ( auto inTreeMaybe = TryCheckInTree( scene, entity ); not inTreeMaybe )
        return inTreeMaybe;
    if ( entity == scene.Root() )
        return unexpected( "the root stays: it goes with the scene"s );
    return TryDo( document, CreateScope<DestroyEntityCommand>( document.GetComponentTypes(), entity ),
                  format( "destroy {}", NameOf( scene, entity ).View() ) );
}

expected<void, string> TryMoveEntity( Document& document, Entity entity, Entity parent, size_t index )
{
    // TrySetParent checks the rest, changing nothing when it refuses.
    return TryDo( document, CreateScope<MoveEntityCommand>( entity, parent, index ),
                  format( "move {}", NameOf( document.GetScene(), entity ).View() ) );
}

expected<Text, string> TryRenameEntity( Document& document, Entity entity, const Text& name )
{
    const Scene& scene = document.GetScene();
    if ( auto inTreeMaybe = TryCheckInTree( scene, entity ); not inTreeMaybe )
        return unexpected( std::move( inTreeMaybe.error() ) );
    if ( entity == scene.Root() )
        return unexpected( "the root has no name to change"s );
    const string label = format( "rename {}", NameOf( scene, entity ).View() );
    if ( auto doneMaybe = TryDo( document, CreateScope<RenameEntityCommand>( entity, name ), label ); not doneMaybe )
        return unexpected( std::move( doneMaybe.error() ) );
    return NameOf( scene, entity );
}

expected<void, string> TryAddComponent( Document& document, Entity entity, Name component )
{
    if ( auto checkedMaybe = TryCheckHasNot( document, entity, component ); not checkedMaybe )
        return checkedMaybe;
    return TryDo( document, CreateScope<AddComponentCommand>( document.GetComponentTypes(), entity, component ),
                  format( "add {}", component.ToString() ) );
}

expected<void, string> TryRemoveComponent( Document& document, Entity entity, Name component )
{
    if ( auto checkedMaybe = TryCheckHas( document, entity, component ); not checkedMaybe )
        return checkedMaybe;
    return TryDo( document, CreateScope<RemoveComponentCommand>( document.GetComponentTypes(), entity, component ),
                  format( "remove {}", component.ToString() ) );
}
}
