#include "prefabs.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
namespace
{
// The roots of the instances of the prefab in `scene`.
vector<Entity> InstancesOf( Scene& scene, const AssetId& prefabId )
{
    vector<Entity> roots;
    scene.ForEach<PrefabInstance>( [&]( Entity entity, const PrefabInstance& instance ) {
        if ( instance.mPrefab == prefabId )
            roots.push_back( entity );
    } );
    return roots;
}

expected<void, string> TryRebuild( Workspace& workspace,
                                   const AssetPath& path,
                                   const AssetId& prefabId,
                                   const Prefab& old,
                                   const Prefab& updated,
                                   OptRef<const Document> skip )
{
    const ComponentTypes& componentTypes = workspace.GetComponentTypes();
    const FileContext files = workspace.GetFileContext();
    for ( const Scope<Document>& open : workspace.All() )
    {
        Document& document = *open;
        if ( skip and &*skip == &document )
            continue;
        Scene& scene = document.SceneOutsideHistory();
        const auto ownPathMaybe = document.TryPath();
        if ( document.TryPrefabNodes() and ownPathMaybe and *ownPathMaybe == path )
        {
            PrefabNodes nodes;
            scene = SceneOf( updated, componentTypes, nodes.mIds );
            nodes.mNextId = updated.NextId();
            document.SetPrefabNodes( std::move( nodes ) );
            // What it held were entities of the scene that went.
            document.GetSelection().Clear();
            document.ForgetHistory();
            continue;
        }
        const vector<Entity> roots = InstancesOf( scene, prefabId );
        if ( roots.empty() )
            continue;
        for ( const Entity root : roots )
        {
            // One inside another made again went with it, and comes back made anew.
            if ( not scene.Alive( root ) )
                continue;
            auto overridesMaybe = TryInstanceOverrides( scene, root, old, files );
            if ( not overridesMaybe )
                return unexpected( std::move( overridesMaybe.error() ) );
            const Text name = NameOf( scene, root );
            const Entity parent = ParentOf( scene, root );
            const size_t index = IndexInParent( scene, root );
            scene.Destroy( root );
            const Entity remade = Instantiate( scene, updated, prefabId, parent, name, componentTypes, index );
            if ( auto appliedMaybe = TryApplyOverrides( scene, remade, *overridesMaybe, files ); not appliedMaybe )
                return appliedMaybe;
        }
        document.ForgetHistory();
    }
    return {};
}
}

bool IsPrefabPath( const AssetPath& path )
{
    return path.Extension() == ".prefab";
}

expected<void, string> TryWritePrefab( Workspace& workspace,
                                       const AssetPath& path,
                                       Ref<const Prefab> prefab,
                                       OptRef<const Document> skip )
{
    const AssetId prefabId = workspace.IdOf( path );
    // What there was: what instances open now were made from.
    const auto oldMaybe = workspace.TryPrefab( prefabId );
    auto textMaybe = prefab->TryWrite( workspace.GetFileContext(), prefabId );
    if ( not textMaybe )
        return unexpected( format( "{}: {}", path.View(), textMaybe.error() ) );
    if ( auto writtenMaybe = workspace.GetFiles().TryWrite( path, *textMaybe ); not writtenMaybe )
        return unexpected( format( "{}: {}", path.View(), writtenMaybe.error() ) );
    workspace.SetPrefab( prefabId, prefab );
    if ( not oldMaybe )
        return {};
    return TryRebuild( workspace, path, prefabId, **oldMaybe, *prefab, skip );
}

expected<void, string> TrySavePrefabDocument( Workspace& workspace, Document& document, const AssetPath& path )
{
    const auto nodesMaybe = document.TryPrefabNodes();
    if ( not nodesMaybe )
        throw logic_error( "TrySavePrefabDocument of a level's document" );
    if ( not IsPrefabPath( path ) )
        return unexpected( format( "{}: a prefab is saved to a .prefab", path.View() ) );
    const Scene& scene = document.GetScene();
    auto prefabMaybe = Prefab::TryFromScene( scene, scene.Root(), workspace.GetComponentTypes(), nodesMaybe->mIds,
                                             nodesMaybe->mNextId );
    if ( not prefabMaybe )
        return unexpected( std::move( prefabMaybe.error() ) );
    nodesMaybe->mNextId = prefabMaybe->NextId();
    if ( auto writtenMaybe =
             TryWritePrefab( workspace, path, CreateRef<const Prefab>( std::move( *prefabMaybe ) ), document );
         not writtenMaybe )
        return writtenMaybe;
    document.SetPath( path );
    document.MarkSaved();
    return {};
}

expected<void, string> TryOpenPrefabDocument( Workspace& workspace, const AssetPath& path )
{
    const auto prefabIdMaybe = workspace.TryIdOf( path );
    if ( not prefabIdMaybe )
        return unexpected( format( "{}: {}", path.View(), prefabIdMaybe.error() ) );
    auto prefabMaybe = workspace.TryPrefab( *prefabIdMaybe );
    if ( not prefabMaybe )
        return unexpected( format( "{}: {}", path.View(), prefabMaybe.error() ) );
    PrefabNodes nodes;
    Scene scene = SceneOf( **prefabMaybe, workspace.GetComponentTypes(), nodes.mIds );
    nodes.mNextId = ( *prefabMaybe )->NextId();
    Document& document = workspace.Open( std::move( scene ) );
    document.SetPath( path );
    document.SetPrefabNodes( std::move( nodes ) );
    return {};
}
}
