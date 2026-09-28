#pragma once
// The harness the engine tests share: a Project built in memory, a CHECK
// that reports and carries on, and TEST( Name ) to declare a case that the
// runner in test_main.cpp finds on its own. Plain functions and asserts;
// the exit code is the verdict.
#include "engine/pch/pch.hpp"
#include "engine/project/project.hpp"
#include "engine/editing/history.hpp"
#include "engine/editing/commands/tree_commands.hpp"
#include "engine/scene/hierarchy.hpp"
#include <print>

using namespace bubble;

namespace test
{
void Fail( const char* file, int line, const char* expr );
void Register( const char* name, void ( *fn )() );

struct Registrar
{
    Registrar( const char* name, void ( *fn )() ) { Register( name, fn ); }
};

// A project with an empty level and a history to edit it through.
struct Fixture
{
    Project project;
    History history;
    Scene& scene = project.mLevel.mScene;

    Fixture()
    {
        project.mScriptingEngine.SetCurrentState();
    }

    Entity Root() const { return scene.Root(); }
    // A copy: the tree may change under a span into it.
    vector<Entity> Children( Entity entity ) const
    {
        const auto children = ChildrenOf( scene, entity );
        return vector<Entity>( children.begin(), children.end() );
    }
    vector<Entity> Top() const { return Children( Root() ); }

    // An entity of `kind` under `parent` (the root), at (1, 2, 3), through
    // the history.
    Entity Create( EntityKind kind, Entity parent = Entity::Null )
    {
        auto command = CreateScope<CreateEntityCommand>( project, scene, parent, kind, Transform( vec3( 1, 2, 3 ) ) );
        auto* raw = command.get();
        history.Execute( std::move( command ) );
        return raw->Created();
    }
};
}

#define CHECK( cond )                                        \
    do {                                                     \
        if ( not ( cond ) )                                  \
            test::Fail( __FILE__, __LINE__, #cond );         \
    } while ( 0 )

#define TEST( Name )                                                          \
    static void Test##Name();                                                 \
    static test::Registrar sRegistrar##Name( #Name, &Test##Name );            \
    static void Test##Name()

using test::Fixture;
