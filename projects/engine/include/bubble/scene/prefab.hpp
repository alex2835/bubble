#pragma once
#include "bubble/data/json.hpp"
#include "bubble/scene/component_types.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scene/prefab_link.hpp"

namespace bubble
{
// A prefab as its file holds it: a tree of nodes with ids that outlast
// renames, each with its components as values. What instances are made from,
// and what they are compared with when a level is saved: a level keeps an
// instance as the prefab and what differs from it. Absolute links in a prefab
// lead from its root, "/" - the prefab is all the scene it has.
//
//     { "version": 1, "next_id": 3,
//       "root": { "id": 0, "components": { ... },
//                 "children": [ { "id": 1, "name": "door", "components": { ... } } ] } }
//
// next_id is the id the next new node gets: an id once given is never given
// again, so what a level says of a node that went is not taken for another.
class Prefab
{
public:
    struct Node
    {
        u32 mId = 0;
        // Empty for the root: an instance's root is named by its level.
        Text mName;
        // In the order of ComponentTypes.
        vector<pair<Name, Value>> mComponents;
        vector<u32> mChildren;
    };

    // The file, as strict as a scene file.
    static expected<Prefab, string> TryParse( string_view text, const ComponentTypes& componentTypes );
    // `root` and what is under it. An entity `ids` has keeps its id; the
    // others get new ones from `nextId` on, added to `ids`. Links into the
    // subtree come to lead from the prefab's root.
    static expected<Prefab, string> TryFromScene( const Scene& scene,
                                                  Entity root,
                                                  const ComponentTypes& componentTypes,
                                                  hmap<Entity, u32>& ids,
                                                  u32 nextId );
    // Fails on a number a file cannot hold: what saves, loads.
    expected<string, string> TryWrite( const ComponentTypes& componentTypes ) const;

    // Root first, then the rest as the tree goes: a parent before its
    // children.
    span<const Node> Nodes() const { return mNodes; }
    const Node& Root() const { return mNodes.front(); }
    OptRef<const Node> TryFind( u32 id ) const;
    u32 NextId() const { return mNextId; }

private:
    vector<Node> mNodes;
    hmap<u32, size_t> mIndex;
    u32 mNextId = 1;
};

constexpr int cPrefabFileVersion = 1;

// Where prefabs come from when a level is read or written: by path, read and
// parsed - and cached, if whoever gives them likes.
using PrefabSource = function<expected<Ref<const Prefab>, string>( const AssetPath& path )>;

// An instance of the prefab at `path` under `parent`, at `index`, named as
// CreateChild names it: an entity a node, each with a PrefabLink, the root
// with PrefabInstance too. Links inside the prefab lead inside the instance.
Entity Instantiate( Scene& scene,
                    const Prefab& prefab,
                    const AssetPath& path,
                    Entity parent,
                    const Text& name,
                    const ComponentTypes& componentTypes,
                    size_t index = cAtEnd );

// What the instance rooted at `root` has that its prefab does not, as a level
// file keeps it, an empty member left out:
//
//     { "set": { "1": { "light.brightness": 3 } }, "add_components": { "1": { "health": { ... } } },
//       "add_children": { "0": [ { "name": "sign", ... } ] }, "remove": [ "2", "1:light" ] }
//
// A field set to what the prefab has is no override - it follows the prefab.
expected<Json::Object, string> TryInstanceOverrides( const Scene& scene,
                                                     Entity root,
                                                     const Prefab& prefab,
                                                     const ComponentTypes& componentTypes );

// Lays what TryInstanceOverrides wrote on the instance at `root`: removes,
// then sets, then added components and children. What the prefab no longer
// has a node or a component for is dropped, with a warning in the log: the
// level loads, and loses it when it is saved again.
expected<void, string> TryApplyOverrides( Scene& scene,
                                          Entity root,
                                          const Json::Object& overrides,
                                          const ComponentTypes& componentTypes );
}
