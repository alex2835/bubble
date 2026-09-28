// Operators by name, with arguments and a context.
#include "test.hpp"
#include "engine/editing/operators/operator.hpp"
#include "engine/editing/selection.hpp"
#include "engine/editing/clipboard.hpp"
#include "engine/serialization/types_serialization.hpp"
#include <nlohmann/json.hpp>
#include "engine/scene/components/state_component.hpp"
#include "engine/scene/components/transform_component.hpp"

TEST( Operators )
{
    OperatorRegistry::RegisterBuiltins();
    Fixture f;
    Selection selection;
    Clipboard clipboard;
    OperatorContext ctx{ f.project, f.project.mLevel, f.history, selection, clipboard };

    // Unknown names throw, and every builtin is listed
    bool threw = false;
    try { InvokeOperator( "scene.nope", ctx ); } catch ( const std::exception& ) { threw = true; }
    CHECK( threw );
    const auto names = OperatorRegistry::Instance().Names();
    CHECK( std::ranges::find( names, "scene.delete" ) != names.end() );

    // Create by name with arguments; the result is selected
    CHECK( InvokeOperator( "scene.create_node", ctx, { { "type", "Light" }, { "spawn_at", vec3( 4, 5, 6 ) } } ) );
    CHECK( f.Top().size() == 1 );
    const Entity light = f.Top()[0];
    CHECK( selection.IsSingleSelection() and selection.GetSingleEntity() == light );
    CHECK( f.scene.GetComponent<TransformComponent>( light ).mPosition == vec3( 4, 5, 6 ) );

    // A folder, then an entity under it by parent id
    selection.Clear();
    CHECK( InvokeOperator( "scene.create_node", ctx, { { "type", "Folder" } } ) );
    const Entity folder = f.Top()[1];
    CHECK( InvokeOperator( "scene.create_node", ctx, { { "type", "Camera" }, { "parent", (u64)folder } } ) );
    CHECK( f.Children( folder ).size() == 1 );
    // With the folder selected, no parent given lands inside it
    selection.Select( folder, f.scene );
    CHECK( InvokeOperator( "scene.create_node", ctx, { { "type", "Script" } } ) );
    CHECK( f.Children( folder ).size() == 2 );

    // Components on the selected entity
    selection.Select( light, f.scene );
    CHECK( InvokeOperator( "entity.add_component", ctx, { { "component", "State" } } ) );
    CHECK( f.scene.HasComponent<StateComponent>( light ) );
    CHECK( InvokeOperator( "entity.remove_component", ctx, { { "component", "State" } } ) );
    CHECK( not f.scene.HasComponent<StateComponent>( light ) );
    threw = false;
    try { InvokeOperator( "entity.remove_component", ctx, { { "component", "Tag" } } ); } catch ( const std::exception& ) { threw = true; }
    CHECK( threw );

    // Delete polls on the selection
    selection.Clear();
    CHECK( not PollOperator( "scene.delete", ctx ) );
    CHECK( not InvokeOperator( "scene.delete", ctx ) );
    CHECK( f.Top().size() == 2 );
    selection.Select( folder, f.scene );
    CHECK( InvokeOperator( "scene.delete", ctx ) );
    CHECK( f.Top().size() == 1 and selection.IsEmpty() );
    CHECK( InvokeOperator( "history.undo", ctx ) );
    CHECK( f.Top().size() == 2 and f.Children( folder ).size() == 2 );

    // Cut / paste moves; copy / paste duplicates
    selection.Select( light, f.scene );
    CHECK( InvokeOperator( "scene.cut", ctx ) );
    CHECK( InvokeOperator( "scene.paste", ctx, { { "parent", (u64)folder } } ) );
    CHECK( f.Top().size() == 1 and f.Children( folder ).size() == 3 );
    CHECK( not PollOperator( "scene.paste", ctx ) ); // clipboard spent by the move
    selection.Select( light, f.scene );
    CHECK( InvokeOperator( "scene.copy", ctx ) );
    CHECK( InvokeOperator( "scene.paste", ctx, { { "parent", (u64)f.Root() } } ) );
    CHECK( f.Top().size() == 2 );
    CHECK( f.Top()[1] != light );
    CHECK( PollOperator( "scene.paste", ctx ) ); // a copy can be pasted again
    // Not into itself, and the root cannot be cut.
    selection.Select( f.Root(), f.scene );
    CHECK( not PollOperator( "scene.cut", ctx ) );

    // Everything above is undoable in order
    while ( f.history.CanUndo() )
        f.history.Undo();
    CHECK( f.Top().empty() and f.scene.EntityCount() == 1 );
}
