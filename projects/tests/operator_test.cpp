// Operators by name, with arguments and a context.
#include "test.hpp"
#include "engine/editing/operator.hpp"
#include "engine/editing/selection.hpp"
#include "engine/editing/clipboard.hpp"
#include "engine/serialization/types_serialization.hpp"
#include <nlohmann/json.hpp>

TEST( Operators )
{
    OperatorRegistry::RegisterBuiltins();
    Fixture f;
    Selection selection;
    Clipboard clipboard;
    OperatorContext ctx{ f.project, f.history, selection, clipboard };

    // Unknown names throw, and every builtin is listed
    bool threw = false;
    try { InvokeOperator( "scene.nope", ctx ); } catch ( const std::exception& ) { threw = true; }
    CHECK( threw );
    const auto names = OperatorRegistry::Instance().Names();
    CHECK( std::ranges::find( names, "scene.delete" ) != names.end() );

    // Create by name with arguments; the result is selected
    CHECK( InvokeOperator( "scene.create_node", ctx, { { "type", "Light" }, { "spawn_at", vec3( 4, 5, 6 ) } } ) );
    CHECK( f.root->mChildren.size() == 1 );
    const auto light = f.root->mChildren[0];
    CHECK( selection.GetTreeNode() == light );
    CHECK( f.scene.GetComponent<TransformComponent>( light->AsEntity() ).mPosition == vec3( 4, 5, 6 ) );

    // A folder, then a node under it by parent id
    CHECK( InvokeOperator( "scene.create_node", ctx, { { "type", "Folder" } } ) );
    const auto folder = f.root->mChildren[1];
    CHECK( InvokeOperator( "scene.create_node", ctx, { { "type", "Camera" }, { "parent", folder->ID() } } ) );
    CHECK( folder->mChildren.size() == 1 );
    // With the folder selected, no parent given lands inside it
    selection.SelectTreeNode( folder, f.scene );
    CHECK( InvokeOperator( "scene.create_node", ctx, { { "type", "Script" } } ) );
    CHECK( folder->mChildren.size() == 2 );

    // Components on the selected entity
    selection.SelectTreeNode( light, f.scene );
    CHECK( InvokeOperator( "entity.add_component", ctx, { { "component", "State" } } ) );
    CHECK( f.scene.HasComponent<StateComponent>( light->AsEntity() ) );
    CHECK( InvokeOperator( "entity.remove_component", ctx, { { "component", "State" } } ) );
    CHECK( not f.scene.HasComponent<StateComponent>( light->AsEntity() ) );
    threw = false;
    try { InvokeOperator( "entity.remove_component", ctx, { { "component", "Tag" } } ); } catch ( const std::exception& ) { threw = true; }
    CHECK( threw );

    // Delete polls on the selection
    selection.Clear();
    CHECK( not PollOperator( "scene.delete", ctx ) );
    CHECK( not InvokeOperator( "scene.delete", ctx ) );
    CHECK( f.root->mChildren.size() == 2 );
    selection.SelectTreeNode( folder, f.scene );
    CHECK( InvokeOperator( "scene.delete", ctx ) );
    CHECK( f.root->mChildren.size() == 1 and selection.IsEmpty() );
    CHECK( InvokeOperator( "history.undo", ctx ) );
    CHECK( f.root->mChildren.size() == 2 and folder->mChildren.size() == 2 );

    // Cut / paste moves; copy / paste duplicates
    selection.SelectTreeNode( light, f.scene );
    CHECK( InvokeOperator( "scene.cut", ctx ) );
    CHECK( InvokeOperator( "scene.paste", ctx, { { "parent", folder->ID() } } ) );
    CHECK( f.root->mChildren.size() == 1 and folder->mChildren.size() == 3 );
    CHECK( not PollOperator( "scene.paste", ctx ) ); // clipboard spent by the move
    selection.SelectTreeNode( light, f.scene );
    CHECK( InvokeOperator( "scene.copy", ctx ) );
    CHECK( InvokeOperator( "scene.paste", ctx, { { "parent", f.root->ID() } } ) );
    CHECK( f.root->mChildren.size() == 2 );
    CHECK( f.root->mChildren[1]->AsEntity() != light->AsEntity() );
    CHECK( PollOperator( "scene.paste", ctx ) ); // a copy can be pasted again

    // Everything above is undoable in order
    while ( f.history.CanUndo() )
        f.history.Undo();
    CHECK( f.root->mChildren.empty() and f.scene.Size() == 0 );
}
