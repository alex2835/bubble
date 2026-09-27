// Structural edits of the scene tree and the entity set: create, delete,
// copy, move, components - applied, undone and redone.
#include "test.hpp"
#include "engine/scene/component_manager.hpp"
#include "engine/editing/commands/property_command.hpp"
#include "engine/editing/commands/component_commands.hpp"
#include <sol/sol.hpp>
#include "engine/scene/components/folder_component.hpp"
#include "engine/scene/components/model_component.hpp"
#include "engine/scene/components/shader_component.hpp"
#include "engine/scene/components/state_component.hpp"
#include "engine/scene/components/tag_component.hpp"
#include "engine/scene/components/transform_component.hpp"

TEST( CreateUndoRedo )
{
    Fixture f;
    // A new level is its root and nothing else.
    CHECK( f.scene.Size() == 1 and f.scene.HasComponent<FolderComponent>( f.Root() ) );

    const Entity entity = f.Create( EntityKind::ModelObject );
    CHECK( f.Top() == vector{ entity } );
    CHECK( ParentOf( f.scene, entity ) == f.Root() );
    CHECK( f.scene.HasComponent<ModelComponent>( entity ) );
    CHECK( f.scene.GetComponent<TransformComponent>( entity ).mPosition == vec3( 1, 2, 3 ) );
    CHECK( f.history.NextUndoName() == "Create ModelObject" );

    f.history.Undo();
    CHECK( f.Top().empty() );
    CHECK( not f.scene.HasEntity( entity ) );

    f.history.Redo();
    CHECK( f.Top() == vector{ entity } ); // same id, so later commands still name it
    CHECK( f.scene.GetComponent<TransformComponent>( entity ).mPosition == vec3( 1, 2, 3 ) );

    // A second round trip: the backup must not hold a stale copy.
    f.history.Undo();
    f.history.Redo();
    CHECK( f.scene.HasEntity( entity ) );
    CHECK( f.scene.HasComponent<ShaderComponent>( entity ) );
}

TEST( FolderAndDelete )
{
    Fixture f;

    const Entity folder = f.Create( EntityKind::Folder );
    CHECK( f.scene.HasComponent<FolderComponent>( folder ) and f.scene.HasComponent<TransformComponent>( folder ) );

    // Two objects inside the folder
    const Entity light = f.Create( EntityKind::Light, folder );
    const Entity camera = f.Create( EntityKind::Camera, folder );
    f.scene.GetComponent<TagComponent>( light ).mName = "my light";
    CHECK( f.Children( folder ) == ( vector{ light, camera } ) );

    f.history.Execute( CreateScope<DeleteEntitiesCommand>( f.scene, vector{ folder } ) );
    CHECK( f.Top().empty() );
    CHECK( not f.scene.HasEntity( light ) and not f.scene.HasEntity( camera ) );

    f.history.Undo();
    CHECK( f.Top() == vector{ folder } );
    CHECK( f.Children( folder ) == ( vector{ light, camera } ) );
    CHECK( f.scene.GetComponent<TagComponent>( light ).mName == "my light" );
    CHECK( ParentOf( f.scene, light ) == folder );

    f.history.Redo();
    CHECK( not f.scene.HasEntity( light ) );
    f.history.Undo();
    CHECK( f.scene.GetComponent<TagComponent>( light ).mName == "my light" );

    // Undo all the way back, then redo all the way forward.
    f.history.Undo(); f.history.Undo(); f.history.Undo();
    CHECK( f.Top().empty() and f.scene.Size() == 1 );
    f.history.Redo(); f.history.Redo(); f.history.Redo(); f.history.Redo();
    CHECK( f.Top().empty() );
    CHECK( not f.history.CanRedo() );

    // The root is never deleted.
    f.history.Execute( CreateScope<DeleteEntitiesCommand>( f.scene, vector{ f.Root() } ) );
    CHECK( f.scene.HasEntity( f.Root() ) );
}

TEST( DeleteMultiple )
{
    Fixture f;
    const Entity n1 = f.Create( EntityKind::Light );
    const Entity n2 = f.Create( EntityKind::Camera );
    const Entity n3 = f.Create( EntityKind::Script );
    const Entity under = f.Create( EntityKind::Light, n3 );

    // One under another that goes too is not deleted twice.
    f.history.Execute( CreateScope<DeleteEntitiesCommand>( f.scene, vector{ n1, n3, under } ) );
    CHECK( f.Top() == vector{ n2 } );

    f.history.Undo();
    CHECK( f.Top() == ( vector{ n1, n2, n3 } ) );
    CHECK( f.Children( n3 ) == vector{ under } );
}

TEST( AddRemoveComponent )
{
    Fixture f;
    const Entity entity = f.Create( EntityKind::ModelObject );

    f.history.Execute( CreateScope<AddComponentCommand>( entity, StateComponent::ID(), f.project, f.scene ) );
    CHECK( f.scene.HasComponent<StateComponent>( entity ) );
    CHECK( f.scene.GetComponent<StateComponent>( entity ).mState->is<Table>() );
    CHECK( f.history.NextUndoName() == "Add State" );
    f.history.Undo();
    CHECK( not f.scene.HasComponent<StateComponent>( entity ) );
    f.history.Redo();
    CHECK( f.scene.HasComponent<StateComponent>( entity ) );

    // Removing keeps the value for undo
    f.scene.GetComponent<TransformComponent>( entity ).mScale = vec3( 7 );
    f.history.Execute( CreateScope<RemoveComponentCommand>( entity, TransformComponent::ID(), f.scene ) );
    CHECK( not f.scene.HasComponent<TransformComponent>( entity ) );
    f.history.Undo();
    CHECK( f.scene.HasComponent<TransformComponent>( entity ) );
    CHECK( f.scene.GetComponent<TransformComponent>( entity ).mScale == vec3( 7 ) );
    f.history.Redo();
    CHECK( not f.scene.HasComponent<TransformComponent>( entity ) );
    f.history.Undo();
    CHECK( f.scene.GetComponent<TransformComponent>( entity ).mScale == vec3( 7 ) );
}

TEST( NewEditForksRedo )
{
    Fixture f;
    f.Create( EntityKind::Light );
    f.history.Undo();
    CHECK( f.history.CanRedo() );
    f.Create( EntityKind::Camera );
    CHECK( not f.history.CanRedo() );
    CHECK( f.Top().size() == 1 );
}

TEST( CopyRedoKeepsIds )
{
    Fixture f;
    const Entity light = f.Create( EntityKind::Light );
    const Entity child = f.Create( EntityKind::Camera, light );

    auto copy = CreateScope<CopyEntityCommand>( f.scene, light, f.Root() );
    auto* raw = copy.get();
    f.history.Execute( std::move( copy ) );
    const Entity pasted = raw->Copy();
    CHECK( f.Top() == ( vector{ light, pasted } ) );
    CHECK( pasted != light );
    // What was under it came along, linked to the copy.
    const auto pastedChildren = f.Children( pasted );
    CHECK( pastedChildren.size() == 1 and pastedChildren[0] != child );
    CHECK( ParentOf( f.scene, pastedChildren[0] ) == pasted );

    // An edit on the copy, then undo past the paste and redo back over it
    using SetPos = SetPropertyCommand<TransformComponent, vec3>;
    f.history.Execute( CreateScope<SetPos>( f.scene, pasted, "Transform.Position", vec3( 1, 2, 3 ), vec3( 7 ),
                                            []( TransformComponent& c, const vec3& v ) { c.mPosition = v; } ) );
    f.history.Undo(); // position
    f.history.Undo(); // paste
    CHECK( not f.scene.HasEntity( pasted ) and f.Top().size() == 1 );
    f.history.Redo(); // paste: same entity id, same place
    CHECK( f.Top() == ( vector{ light, pasted } ) );
    CHECK( f.Children( pasted ) == pastedChildren );
    f.history.Redo(); // position lands on the copy again
    CHECK( f.scene.GetComponent<TransformComponent>( pasted ).mPosition == vec3( 7 ) );
}

TEST( MoveKeepsOrder )
{
    Fixture f;
    const Entity a = f.Create( EntityKind::Light );
    const Entity b = f.Create( EntityKind::Light );
    const Entity c = f.Create( EntityKind::Light );

    // To the front among the same siblings, then back.
    f.history.Execute( CreateScope<MoveEntityCommand>( f.scene, c, f.Root(), 0 ) );
    CHECK( f.Top() == ( vector{ c, a, b } ) );
    f.history.Undo();
    CHECK( f.Top() == ( vector{ a, b, c } ) );
    // Down among them: the index is where it ends up counted before it moved.
    f.history.Execute( CreateScope<MoveEntityCommand>( f.scene, a, f.Root(), 2 ) );
    CHECK( f.Top() == ( vector{ b, a, c } ) );
}
