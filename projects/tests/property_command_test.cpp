// One property of one component, set and recorded.
#include "test.hpp"
#include "engine/editing/property_command.hpp"
#include <sol/sol.hpp>

TEST( SetProperty )
{
    Fixture f;
    auto node = f.Create( ProjectTreeNodeType::Camera );
    const Entity entity = node->AsEntity();

    using SetPos = SetPropertyCommand<TransformComponent, vec3>;
    f.history.Execute( CreateScope<SetPos>( f.scene, entity, "Transform.Position", vec3( 1, 2, 3 ), vec3( 9, 9, 9 ),
                                            []( TransformComponent& c, const vec3& v ) { c.mPosition = v; } ) );
    CHECK( f.scene.GetComponent<TransformComponent>( entity ).mPosition == vec3( 9 ) );
    CHECK( f.history.NextUndoName() == "Transform.Position" );
    f.history.Undo();
    CHECK( f.scene.GetComponent<TransformComponent>( entity ).mPosition == vec3( 1, 2, 3 ) );
    f.history.Redo();
    CHECK( f.scene.GetComponent<TransformComponent>( entity ).mPosition == vec3( 9 ) );

    // Record: the widget already wrote the value; only undo/redo apply.
    f.scene.GetComponent<TagComponent>( entity ).mName = "typed";
    using SetName = SetPropertyCommand<TagComponent, string>;
    f.history.Record( CreateScope<SetName>( f.scene, entity, "Tag.Name", "Camera"s, "typed"s,
                                            []( TagComponent& c, const string& v ) { c.mName = v; } ) );
    CHECK( f.scene.GetComponent<TagComponent>( entity ).mName == "typed" );
    f.history.Undo();
    CHECK( f.scene.GetComponent<TagComponent>( entity ).mName == "Camera" );
    f.history.Redo();
    CHECK( f.scene.GetComponent<TagComponent>( entity ).mName == "typed" );

    // A property step on an entity that is deleted and restored still lands.
    f.history.Execute( CreateScope<DeleteNodeCommand>( node, f.scene ) );
    f.history.Undo(); // restore
    f.history.Undo(); // name back to Camera
    CHECK( f.scene.GetComponent<TagComponent>( entity ).mName == "Camera" );
}
