#pragma once
// The harness the engine tests share: a Project built in memory, a CHECK
// that reports and carries on, and TEST( Name ) to declare a case that the
// runner in test_main.cpp finds on its own. Plain functions and asserts;
// the exit code is the verdict.
#include "engine/pch/pch.hpp"
#include "engine/project/project.hpp"
#include "engine/editing/history.hpp"
#include "engine/editing/commands/tree_commands.hpp"
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
    Ref<ProjectTreeNode>& root = project.mLevel.mTreeRoot;

    Fixture()
    {
        project.mScriptingEngine.SetCurrentState();
    }

    // A node of `type` under the root, at (1, 2, 3), through the history.
    Ref<ProjectTreeNode> Create( ProjectTreeNodeType type )
    {
        auto command = CreateScope<CreateNodeCommand>( root, type, project, Transform( vec3( 1, 2, 3 ) ) );
        auto* raw = command.get();
        history.Execute( std::move( command ) );
        return raw->GetCreatedNode();
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
