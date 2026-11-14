#include "bubble/editing/edits.hpp"
#include "bubble/core/assert.hpp"
#include "bubble/scene/scene_file.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include "links.hpp"
#include <typeinfo>

namespace bubble
{
namespace
{
// A type the edit found, looked up again: a command keeps the name, as the
// list of types may grow and move.
const ComponentTypes::Type& TypeNamed( const ComponentTypes& componentTypes, Name component )
{
    const auto typeMaybe = componentTypes.TryFindAny( component );
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

    // A type with no == compares by address: then a set counts as a change.
    bool Changed() const override { return not( mBefore == mAfter ); }

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

// An entity with everything under it, taken out of the scene and kept as it
// was, to be put back as it was: under the same numbers, in the same places,
// with copies of its components - of the types ComponentTypes knows; the
// rest was a cache, rebuilt.
class TakenSubtree
{
public:
    explicit TakenSubtree( const ComponentTypes& componentTypes ) : mComponentTypes( componentTypes ) {}

    void TakeOut( Scene& scene, Entity entity )
    {
        Keep( scene, entity );
        scene.Destroy( entity );
    }

    // The scene is as TakeOut left it.
    void PutBack( Scene& scene )
    {
        // Parents before their children, each child last so far: the order
        // they were kept in puts every one back in its place.
        for ( Kept& kept : mKept )
        {
            scene.Revive( kept.mEntity );
            const Text name = AddToTree( scene, kept.mEntity, kept.mParent, kept.mName, kept.mIndex );
            BUBBLE_ASSERT( name == kept.mName, "an entity put back with another name" );
            for ( auto& [component, value] : kept.mComponents )
                TypeNamed( mComponentTypes, component ).mAdd( scene, kept.mEntity, value );
        }
        mKept.clear();
    }

private:
    // One entity of the subtree as it was.
    struct Kept
    {
        Entity mEntity;
        Entity mParent;
        size_t mIndex = 0;
        Text mName;
        vector<pair<Name, MetaAny>> mComponents;
    };

    void Keep( const Scene& scene, Entity entity )
    {
        Kept kept{ .mEntity = entity,
                   .mParent = ParentOf( scene, entity ),
                   .mIndex = IndexInParent( scene, entity ),
                   .mName = NameOf( scene, entity ),
                   .mComponents = {} };
        // The engine's own as well: undo brings back an instance's node as one.
        for ( const span<const ComponentTypes::Type> types : { mComponentTypes.All(), mComponentTypes.Internal() } )
            for ( const ComponentTypes::Type& type : types )
                if ( const MetaAny component = type.mGet( scene, entity ) )
                    kept.mComponents.emplace_back( type.mName, Copy( component ) );
        mKept.push_back( std::move( kept ) );
        for ( const Entity child : ChildrenOf( scene, entity ) )
            Keep( scene, child );
    }

    const ComponentTypes& mComponentTypes;
    vector<Kept> mKept;
};

class DestroyEntityCommand final : public Command
{
public:
    DestroyEntityCommand( const ComponentTypes& componentTypes, Entity entity )
        : mEntity( entity ),
          mTaken( componentTypes )
    {
    }

    expected<void, string> TryDo( Scene& scene ) override
    {
        mTaken.TakeOut( scene, mEntity );
        return {};
    }

    void Undo( Scene& scene ) override { mTaken.PutBack( scene ); }

private:
    Entity mEntity;
    TakenSubtree mTaken;
};

// Entities made from what a file holds - a paste, a duplicate - under their
// parents. Undo takes them out, and redo puts the same ones back.
class PasteCommand final : public Command
{
public:
    struct Piece
    {
        Json mEntity;
        // Where it was copied from: absolute links into it lead into what is
        // made of it.
        EntityPath mFrom;
        Entity mParent;
        // Right after this entity when there is one, else at mIndex.
        Entity mAfter;
        size_t mIndex = cAtEnd;
    };

    PasteCommand( const ComponentTypes& componentTypes, vector<Piece> pieces )
        : mComponentTypes( componentTypes ),
          mPieces( std::move( pieces ) )
    {
    }

    span<const Entity> Made() const { return mMade; }

    expected<void, string> TryDo( Scene& scene ) override
    {
        for ( const Piece& piece : mPieces )
        {
            const size_t index = piece.mAfter != Entity::Null ? IndexInParent( scene, piece.mAfter ) + 1 : piece.mIndex;
            auto madeMaybe = TryEntityFromJson( scene, piece.mEntity, piece.mParent, mComponentTypes, index );
            if ( not madeMaybe )
            {
                for ( auto made = mMade.rbegin(); made != mMade.rend(); ++made )
                    scene.Destroy( *made );
                mMade.clear();
                return unexpected( std::move( madeMaybe.error() ) );
            }
            mMade.push_back( *madeMaybe );
        }
        LeadLinksIntoCopies( scene );
        // From here on undo keeps what was made.
        mPieces.clear();
        for ( size_t i = 0; i < mMade.size(); ++i )
            mTaken.emplace_back( mComponentTypes );
        return {};
    }

    // The last made first: putting them back in the order they were made
    // then finds each place as it was.
    void Undo( Scene& scene ) override
    {
        for ( size_t i = mMade.size(); i-- > 0; )
            mTaken[i].TakeOut( scene, mMade[i] );
    }

    void Redo( Scene& scene ) override
    {
        for ( TakenSubtree& taken : mTaken )
            taken.PutBack( scene );
    }

private:
    void LeadLinksIntoCopies( Scene& scene )
    {
        vector<EntityPath> madeAt;
        for ( const Entity made : mMade )
            madeAt.push_back( PathOf( scene, made ) );
        for ( const Entity made : mMade )
            for ( const EntityLink& link : FindLinks( scene, mComponentTypes, made ) )
            {
                if ( not link.mLink.IsAbsolute() )
                    continue;
                for ( size_t i = 0; i < mPieces.size(); ++i )
                    if ( const auto rebasedMaybe = TryRebase( link.mLink, mPieces[i].mFrom, madeAt[i] ) )
                    {
                        SetFieldCommand set( mComponentTypes, link.mHolder, link.mComponent, link.mPath,
                                             MetaAny( Value( *rebasedMaybe ) ) );
                        if ( auto setMaybe = set.TryDo( scene ); not setMaybe )
                            throw logic_error( format( "a link set where one was: {}", setMaybe.error() ) );
                        break;
                    }
            }
    }

    const ComponentTypes& mComponentTypes;
    vector<Piece> mPieces;
    vector<Entity> mMade;
    vector<TakenSubtree> mTaken;
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
        // The root has no place to keep; TrySetParent refuses to move it.
        mOldIndex = mOldParent != Entity::Null ? IndexInParent( scene, mEntity ) : 0;
        mOldName = NameOf( scene, mEntity );
        if ( auto movedMaybe = TrySetParent( scene, mEntity, mParent, mIndex ); not movedMaybe )
            return movedMaybe;
        mMoved = mParent != mOldParent or IndexInParent( scene, mEntity ) != mOldIndex or
                 NameOf( scene, mEntity ) != mOldName;
        return {};
    }

    bool Changed() const override { return mMoved; }

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
    bool mMoved = false;
};

class RenameEntityCommand final : public Command
{
public:
    RenameEntityCommand( Entity entity, Text name ) : mEntity( entity ), mWanted( std::move( name ) ) {}

    expected<void, string> TryDo( Scene& scene ) override
    {
        mOldName = NameOf( scene, mEntity );
        mRenamed = Rename( scene, mEntity, mWanted ) != mOldName;
        return {};
    }

    void Undo( Scene& scene ) override { Rename( scene, mEntity, mOldName ); }
    bool Changed() const override { return mRenamed; }

private:
    Entity mEntity;
    Text mWanted;
    Text mOldName;
    bool mRenamed = false;
};

// A change to the tree - a rename, a move - with the links it broke set to
// lead where they led, in the same step: an absolute link stays absolute, a
// relative one relative. A link that led nowhere is left as it is.
class KeepLinksCommand final : public Command
{
public:
    KeepLinksCommand( const ComponentTypes& componentTypes, Scope<Command> change )
        : mComponentTypes( componentTypes ),
          mChange( std::move( change ) )
    {
    }

    expected<void, string> TryDo( Scene& scene ) override
    {
        const vector<EntityLink> links = FindLinks( scene, mComponentTypes, scene.Root() );
        if ( auto doneMaybe = mChange->TryDo( scene ); not doneMaybe )
            return doneMaybe;
        for ( const EntityLink& link : links )
        {
            if ( link.mTarget == Entity::Null )
                continue;
            if ( const auto nowMaybe = TryResolve( scene, link.mLink, link.mHolder );
                 nowMaybe and *nowMaybe == link.mTarget )
                continue;
            const EntityPath kept = link.mLink.IsAbsolute() ? PathOf( scene, link.mTarget )
                                                            : RelativePath( scene, link.mHolder, link.mTarget );
            // As a Value, which goes into an EntityPath field and into a
            // Value's entity alike.
            auto fix = CreateScope<SetFieldCommand>( mComponentTypes, link.mHolder, link.mComponent, link.mPath,
                                                     MetaAny( Value( kept ) ) );
            if ( auto fixedMaybe = fix->TryDo( scene ); not fixedMaybe )
                throw logic_error( format( "a link set where one was: {}", fixedMaybe.error() ) );
            mFixes.push_back( std::move( fix ) );
        }
        return {};
    }

    void Undo( Scene& scene ) override
    {
        for ( auto fix = mFixes.rbegin(); fix != mFixes.rend(); ++fix )
            ( *fix )->Undo( scene );
        mChange->Undo( scene );
    }

    void Redo( Scene& scene ) override
    {
        mChange->Redo( scene );
        for ( const Scope<Command>& fix : mFixes )
            fix->Redo( scene );
    }

    bool Changed() const override { return mChange->Changed(); }

private:
    const ComponentTypes& mComponentTypes;
    Scope<Command> mChange;
    vector<Scope<Command>> mFixes;
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

expected<vector<Entity>, string> TryPaste( Document& document, vector<PasteCommand::Piece> pieces, string label )
{
    auto command = CreateScope<PasteCommand>( document.GetComponentTypes(), std::move( pieces ) );
    const PasteCommand& paste = *command;
    if ( auto doneMaybe = TryDo( document, std::move( command ), std::move( label ) ); not doneMaybe )
        return unexpected( std::move( doneMaybe.error() ) );
    // Kept by the history, which does not drop a step it has just taken.
    return vector<Entity>( paste.Made().begin(), paste.Made().end() );
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
    return TryDo( document,
                  CreateScope<KeepLinksCommand>( document.GetComponentTypes(),
                                                 CreateScope<MoveEntityCommand>( entity, parent, index ) ),
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
    auto rename =
        CreateScope<KeepLinksCommand>( document.GetComponentTypes(), CreateScope<RenameEntityCommand>( entity, name ) );
    if ( auto doneMaybe = TryDo( document, std::move( rename ), label ); not doneMaybe )
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

expected<vector<Entity>, string> TryPasteEntities( Document& document,
                                                   const Clipboard& clipboard,
                                                   Entity parent,
                                                   size_t index )
{
    if ( auto inTreeMaybe = TryCheckInTree( document.GetScene(), parent ); not inTreeMaybe )
        return unexpected( std::move( inTreeMaybe.error() ) );
    if ( clipboard.Empty() )
        return unexpected( "nothing to paste: the clipboard is empty"s );
    vector<PasteCommand::Piece> pieces;
    for ( const Clipboard::CopiedEntity& copied : clipboard.All() )
        pieces.push_back( PasteCommand::Piece{ .mEntity = copied.mEntity,
                                               .mFrom = copied.mFrom,
                                               .mParent = parent,
                                               .mAfter = Entity::Null,
                                               .mIndex = index == cAtEnd ? cAtEnd : index + pieces.size() } );
    return TryPaste( document, std::move( pieces ), "paste" );
}

expected<vector<Entity>, string> TryDuplicateEntities( Document& document, span<const Entity> entities )
{
    const Scene& scene = document.GetScene();
    vector<PasteCommand::Piece> pieces;
    for ( const Entity entity : entities )
    {
        if ( auto inTreeMaybe = TryCheckInTree( scene, entity ); not inTreeMaybe )
            return unexpected( std::move( inTreeMaybe.error() ) );
        if ( entity == scene.Root() )
            return unexpected( "the root is not duplicated: it is the scene"s );
        auto jsonMaybe = TryEntityToJson( scene, entity, document.GetComponentTypes() );
        if ( not jsonMaybe )
            return unexpected( std::move( jsonMaybe.error() ) );
        pieces.push_back( PasteCommand::Piece{ .mEntity = std::move( *jsonMaybe ),
                                               .mFrom = PathOf( scene, entity ),
                                               .mParent = ParentOf( scene, entity ),
                                               .mAfter = entity,
                                               .mIndex = cAtEnd } );
    }
    if ( pieces.empty() )
        return unexpected( "nothing to duplicate"s );
    return TryPaste( document, std::move( pieces ), "duplicate" );
}
}
