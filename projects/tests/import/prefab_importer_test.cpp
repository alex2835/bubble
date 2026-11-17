// The prefab importer: a .prefab kept as it is, once it reads as a prefab
// of the project's component types.
#include "bubble/import/prefab_importer.hpp"
#include "bubble/world/app.hpp"
#include "bubble/world/transform.hpp"
#include <doctest.h>

using namespace bubble;

TEST_CASE( "A prefab is imported as it is, once it reads; one that does not is refused with its path" )
{
    App app;
    app.AddModule( TransformModule() );
    AssetCatalog catalog;
    const Importer importer = PrefabImporter( app.GetComponentTypes(), catalog );
    CHECK( importer.mExtension == ".prefab" );
    const string good =
        R"({ "version": 1, "next_id": 1, "root": { "id": 0, "components": { "transform": { "position": [ 1, 2, 3 ] } } } })";
    const auto importedMaybe = importer.mImport( good, Json(), *AssetPath::TryFrom( "prefabs/crate.prefab" ) );
    REQUIRE_MESSAGE( importedMaybe, importedMaybe.error() );
    CHECK( importedMaybe->mBytes == good );

    const auto brokenMaybe =
        importer.mImport( R"({ "version": 1, "next_id": 1, "root": { "id": 0, "components": { "lamp": {} } } })",
                          Json(), *AssetPath::TryFrom( "prefabs/lamp.prefab" ) );
    REQUIRE_FALSE( brokenMaybe );
    CHECK( brokenMaybe.error().starts_with( "prefabs/lamp.prefab: " ) );
    CHECK( brokenMaybe.error().find( "lamp is not a component" ) != string::npos );
}
