// Structural edits of the tree and the entity set: create, delete, copy,
// move, components - applied, undone and redone.
#include "test.hpp"
#include "engine/scene/component_manager.hpp"
#include "engine/editing/commands/property_command.hpp"
#include "engine/editing/commands/component_commands.hpp"
#include <sol/sol.hpp>

TEST( CreateUndoRedo )
{
    Fixture f;

    auto node = f.Create( ProjectTreeNodeType::ModelObject );
    const Entity entity = node->AsEntity();
    CHECK( f.root->mChildren.size() == 1 );
    CHECK( f.scene.HasEntity( entity ) );
    CHECK( f.scene.HasComponent<ModelComponent>( entity ) );
    CHECK( f.scene.GetComponent<TransformComponent>( entity ).mPosition == vec3( 1, 2, 3 ) );
    CHECK( f.history.NextUndoName() == "Create ModelObject" );

    f.history.Undo();
    CHECK( f.root->mChildren.empty() );
    CHECK( not f.scene.HasEntity( entity ) );

    f.history.Redo();
    CHECK( f.root->mChildren.size() == 1 );
    CHECK( f.scene.HasEntity( entity ) );
    CHECK( node->AsEntity() == entity ); // same id, so later commands still name it
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

    auto folder = f.Create( ProjectTreeNodeType::Folder );
    CHECK( not folder->IsEntity() );

    // Two objects inside the folder
    auto a = CreateScope<CreateNodeCommand>( folder, ProjectTreeNodeType::Light, f.project, Transform() );
    auto b = CreateScope<CreateNodeCommand>( folder, ProjectTreeNodeType::Camera, f.project, Transform() );
    auto* aRaw = a.get(); auto* bRaw = b.get();
    f.history.Execute( std::move( a ) );
    f.history.Execute( std::move( b ) );
    const Entity light = aRaw->GetCreatedNode()->AsEntity();
    const Entity camera = bRaw->GetCreatedNode()->AsEntity();
    f.scene.GetComponent<TagComponent>( light ).mName = "my light";

    f.history.Execute( CreateScope<DeleteNodeCommand>( folder, f.scene ) );
    CHECK( f.root->mChildren.empty() );
    CHECK( not f.scene.HasEntity( light ) );
    CHECK( not f.scene.HasEntity( camera ) );

    f.history.Undo();
    CHECK( f.root->mChildren.size() == 1 and f.root->mChildren[0] == folder );
    CHECK( folder->mChildren.size() == 2 );
    CHECK( f.scene.HasEntity( light ) and f.scene.HasEntity( camera ) );
    CHECK( f.scene.GetComponent<TagComponent>( light ).mName == "my light" );
    CHECK( folder->mChildren[0]->mParent.lock() == folder );

    f.history.Redo();
    CHECK( not f.scene.HasEntity( light ) );
    f.history.Undo();
    CHECK( f.scene.HasEntity( light ) );
    CHECK( f.scene.GetComponent<TagComponent>( light ).mName == "my light" );

    // Undo all the way back, then redo all the way forward.
    f.history.Undo(); f.history.Undo(); f.history.Undo();
    CHECK( f.root->mChildren.empty() and f.scene.Size() == 0 );
    f.history.Redo(); f.history.Redo(); f.history.Redo(); f.history.Redo();
    CHECK( f.root->mChildren.empty() );
    CHECK( not f.history.CanRedo() );
}

TEST( DeleteMultiple )
{
    Fixture f;
    auto n1 = f.Create( ProjectTreeNodeType::Light );
    auto n2 = f.Create( ProjectTreeNodeType::Camera );
    auto n3 = f.Create( ProjectTreeNodeType::Script );

    f.history.Execute( CreateScope<DeleteMultipleNodesCommand>( vector{ n1, n3 }, f.scene ) );
    CHECK( f.root->mChildren.size() == 1 and f.root->mChildren[0] == n2 );

    f.history.Undo();
    CHECK( f.root->mChildren.size() == 3 );
    CHECK( f.root->mChildren[0] == n1 and f.root->mChildren[1] == n2 and f.root->mChildren[2] == n3 );
    CHECK( f.scene.HasEntity( n1->AsEntity() ) and f.scene.HasEntity( n3->AsEntity() ) );
}

TEST( AddRemoveComponent )
{
    Fixture f;
    auto node = f.Create( ProjectTreeNodeType::ModelObject );
    const Entity entity = node->AsEntity();

    f.history.Execute( CreateScope<AddComponentCommand>( entity, StateComponent::ID(), f.project ) );
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
    f.Create( ProjectTreeNodeType::Light );
    f.history.Undo();
    CHECK( f.history.CanRedo() );
    f.Create( ProjectTreeNodeType::Camera );
    CHECK( not f.history.CanRedo() );
    CHECK( f.root->mChildren.size() == 1 );
}

TEST( CopyRedoKeepsIds )
{
    Fixture f;
    auto light = f.Create( ProjectTreeNodeType::Light );

    f.history.Execute( CreateScope<CopyNodeCommand>( light, f.root, f.scene ) );
    CHECK( f.root->mChildren.size() == 2 );
    const auto pasted = f.root->mChildren[1];
    const Entity pastedEntity = pasted->AsEntity();
    CHECK( pastedEntity != light->AsEntity() );

    // An edit on the copy, then undo past the paste and redo back over it
    using SetPos = SetPropertyCommand<TransformComponent, vec3>;
    f.history.Execute( CreateScope<SetPos>( f.scene, pastedEntity, "Transform.Position", vec3( 1, 2, 3 ), vec3( 7 ),
                                            []( TransformComponent& c, const vec3& v ) { c.mPosition = v; } ) );
    f.history.Undo(); // position
    f.history.Undo(); // paste
    CHECK( not f.scene.HasEntity( pastedEntity ) and f.root->mChildren.size() == 1 );
    f.history.Redo(); // paste: same node, same entity id
    CHECK( f.root->mChildren.size() == 2 and f.root->mChildren[1] == pasted );
    CHECK( pasted->AsEntity() == pastedEntity and f.scene.HasEntity( pastedEntity ) );
    f.history.Redo(); // position lands on the copy again
    CHECK( f.scene.GetComponent<TransformComponent>( pastedEntity ).mPosition == vec3( 7 ) );
}
