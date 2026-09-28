#include "engine/pch/pch.hpp"
#include "engine/scene/hierarchy.hpp"
#include "engine/scene/components/hierarchy_component.hpp"
#include "engine/scene/components/prefab_instance_component.hpp"
#include "engine/scene/components/state_component.hpp"
#include "engine/scene/components/tag_component.hpp"
#include "engine/scene/components/transform_component.hpp"
#include "engine/types/set.hpp"
#include "engine/utils/lookalike_text.hpp"
#include <sol/sol.hpp>

namespace bubble
{
namespace
{
// Deeper than any real hierarchy; past it, the links are taken to be a loop
// that got into a file somehow, and the walk stops instead of the stack.
constexpr int cMaxDepth = 1024;

const HierarchyComponent* FindHierarchy( const Scene& scene, Entity entity )
{
    return scene.HasEntity( entity ) and scene.HasComponent<HierarchyComponent>( entity )
           ? &scene.GetComponent<HierarchyComponent>( entity ) : nullptr;
}

HierarchyComponent& EnsureHierarchy( Scene& scene, Entity entity )
{
    if ( scene.HasComponent<HierarchyComponent>( entity ) )
        return scene.GetComponent<HierarchyComponent>( entity );
    return scene.AddComponent<HierarchyComponent>( entity );
}

void UpdateSubtree( Scene& scene, Entity entity, const mat4& parentWorld, bool hasParent, int depth )
{
    if ( depth > cMaxDepth )
        return;

    // An entity without a transform - a script holder - passes its parent's
    // world on to its children.
    mat4 world = parentWorld;
    if ( scene.HasComponent<TransformComponent>( entity ) )
    {
        auto& transform = scene.GetComponent<TransformComponent>( entity );
        if ( hasParent )
        {
            transform.mWorldMatrix = parentWorld * transform.TransformMat();
            transform.mWorld = Transform::FromMatrix( transform.mWorldMatrix );
        }
        else
        {
            transform.mWorldMatrix = transform.TransformMat();
            transform.mWorld = transform;
        }
        world = transform.mWorldMatrix;
    }

    const auto* hierarchy = FindHierarchy( scene, entity );
    if ( not hierarchy )
        return;
    // Copied: a child's update must not be looking into this pool while it
    // writes its own component, and the list is short.
    const vector<Entity> children = hierarchy->mChildren;
    for ( const Entity child : children )
        if ( scene.HasEntity( child ) )
            UpdateSubtree( scene, child, world, true, depth + 1 );
}

void RemapTable( Table table, const map<Entity, Entity>& copied, set<const void*>& visited )
{
    if ( not visited.insert( table.pointer() ).second )
        return;
    vector<std::pair<sol::object, sol::object>> changes;
    vector<Table> nested;
    for ( const auto& [key, value] : table )
    {
        if ( value.is<Entity>() )
        {
            const auto it = copied.find( value.as<Entity>() );
            changes.emplace_back( key, it != copied.end() ? sol::make_object<Entity>( table.lua_state(), Entity( it->second ) )
                                                          : sol::make_object( table.lua_state(), sol::lua_nil ) );
        }
        else if ( value.get_type() == sol::type::table )
            nested.push_back( value.as<Table>() );
    }
    for ( const auto& [key, value] : changes )
        table.raw_set( key, value );
    for ( const Table& t : nested )
        RemapTable( t, copied, visited );
}
}


Entity ParentOf( const Scene& scene, Entity entity )
{
    const auto* h = FindHierarchy( scene, entity );
    return h ? h->mParent : INVALID_ENTITY;
}

std::span<const Entity> ChildrenOf( const Scene& scene, Entity entity )
{
    const auto* h = FindHierarchy( scene, entity );
    return h ? std::span<const Entity>( h->mChildren ) : std::span<const Entity>();
}

size_t IndexInParent( const Scene& scene, Entity entity )
{
    const auto siblings = ChildrenOf( scene, ParentOf( scene, entity ) );
    const auto it = std::ranges::find( siblings, entity );
    return it == siblings.end() ? cAtEnd : size_t( it - siblings.begin() );
}

bool IsAncestor( const Scene& scene, Entity ancestor, Entity entity )
{
    Entity current = ParentOf( scene, entity );
    for ( int depth = 0; current != INVALID_ENTITY and depth < cMaxDepth; depth++ )
    {
        if ( current == ancestor )
            return true;
        current = ParentOf( scene, current );
    }
    return false;
}

vector<Entity> Subtree( const Scene& scene, Entity entity )
{
    vector<Entity> out;
    std::function<void( Entity, int )> walk = [&]( Entity e, int depth )
    {
        if ( depth > cMaxDepth or not scene.HasEntity( e ) )
            return;
        out.push_back( e );
        for ( const Entity child : ChildrenOf( scene, e ) )
            walk( child, depth + 1 );
    };
    walk( entity, 0 );
    return out;
}

void AttachChild( Scene& scene, Entity child, Entity parent, size_t index )
{
    auto& children = EnsureHierarchy( scene, parent ).mChildren;
    children.insert( children.begin() + std::min( index, children.size() ), child );
    // After the parent's: adding a component may move the pool the other
    // reference points into.
    EnsureHierarchy( scene, child ).mParent = parent;
}

size_t DetachFromParent( Scene& scene, Entity child )
{
    const Entity parent = ParentOf( scene, child );
    size_t index = cAtEnd;
    if ( parent != INVALID_ENTITY and scene.HasEntity( parent ) )
    {
        auto& siblings = scene.GetComponent<HierarchyComponent>( parent ).mChildren;
        if ( const auto it = std::ranges::find( siblings, child ); it != siblings.end() )
        {
            index = size_t( it - siblings.begin() );
            siblings.erase( it );
        }
    }
    EnsureHierarchy( scene, child ).mParent = INVALID_ENTITY;
    return index;
}

bool SetParent( Scene& scene, Entity child, Entity parent, bool keepWorld, size_t index )
{
    if ( parent == INVALID_ENTITY )
        parent = scene.Root();
    if ( child == INVALID_ENTITY or not scene.HasEntity( child ) or child == scene.Root() )
        return false;
    if ( parent == INVALID_ENTITY or parent == child or not scene.HasEntity( parent ) or IsAncestor( scene, child, parent ) )
        return false;

    const mat4 world = ComputeWorldMatrix( scene, child );
    DetachFromParent( scene, child );
    AttachChild( scene, child, parent, index );
    if ( keepWorld )
        SetWorldTransform( scene, child, Transform::FromMatrix( world ) );
    return true;
}

Entity CreateChildEntity( Scene& scene, Entity parent )
{
    const Entity entity = scene.CreateEntity();
    scene.AddComponent<HierarchyComponent>( entity );
    if ( parent == INVALID_ENTITY )
        parent = scene.Root();
    if ( parent != INVALID_ENTITY )
        AttachChild( scene, entity, parent );
    return entity;
}

Entity CopySubtree( Scene& from, Entity entity, Scene& to, map<Entity, Entity>& copied, std::optional<size_t> topId )
{
    const vector<Entity> originals = Subtree( from, entity );
    for ( const Entity original : originals )
    {
        Entity made;
        if ( original == entity and topId )
            made = from.CopyEntityIntoWithId( to, original, *topId );
        else if ( &from == &to )
            made = from.CopyEntity( original );
        else
            made = from.CopyEntityInto( to, original );
        copied[original] = made;
    }
    // The links came across naming the originals.
    for ( const Entity original : originals )
    {
        const Entity made = copied.at( original );
        auto& h = EnsureHierarchy( to, made );
        const auto parent = copied.find( h.mParent );
        h.mParent = original == entity or parent == copied.end() ? INVALID_ENTITY : parent->second;
        vector<Entity> children;
        for ( const Entity child : h.mChildren )
            if ( const auto it = copied.find( child ); it != copied.end() )
                children.push_back( it->second );
        h.mChildren = std::move( children );
    }
    RemapEntityReferences( to, copied );
    return copied.at( entity );
}

void RemapEntityReferences( Scene& scene, const map<Entity, Entity>& copied )
{
    set<const void*> visited;
    for ( const auto& [_, entity] : copied )
    {
        if ( not scene.HasComponent<StateComponent>( entity ) )
            continue;
        const auto& state = scene.GetComponent<StateComponent>( entity ).mState;
        if ( state and state->is<Table>() )
            RemapTable( state->as<Table>(), copied, visited );
    }
}

mat4 ComputeWorldMatrix( const Scene& scene, Entity entity )
{
    mat4 world = scene.HasComponent<TransformComponent>( entity )
                 ? scene.GetComponent<TransformComponent>( entity ).TransformMat()
                 : mat4( 1.0f );
    Entity parent = ParentOf( scene, entity );
    for ( int depth = 0; parent != INVALID_ENTITY and scene.HasEntity( parent ) and depth < cMaxDepth; depth++ )
    {
        if ( scene.HasComponent<TransformComponent>( parent ) )
            world = scene.GetComponent<TransformComponent>( parent ).TransformMat() * world;
        parent = ParentOf( scene, parent );
    }
    return world;
}

void SetWorldTransform( Scene& scene, Entity entity, const Transform& world )
{
    if ( not scene.HasComponent<TransformComponent>( entity ) )
        return;
    const Entity parent = ParentOf( scene, entity );
    Transform local = world;
    if ( parent != INVALID_ENTITY and scene.HasEntity( parent ) )
        local = Transform::FromMatrix( glm::inverse( ComputeWorldMatrix( scene, parent ) ) * world.TransformMat() );
    auto& transform = scene.GetComponent<TransformComponent>( entity );
    transform.mPosition = local.mPosition;
    transform.mRotation = local.mRotation;
    transform.mScale = local.mScale;
}

void UpdateWorldTransforms( Scene& scene )
{
    // From the root; and from anything hanging from nothing, which the tree
    // does not show but which still has a place in the world.
    vector<Entity> tops;
    scene.ForEachEntity( [&]( Entity entity )
    {
        const Entity parent = ParentOf( scene, entity );
        if ( parent == INVALID_ENTITY or not scene.HasEntity( parent ) )
            tops.push_back( entity );
    } );
    for ( const Entity top : tops )
        UpdateSubtree( scene, top, mat4( 1.0f ), false, 0 );
}

// Names and paths

string NameOf( const Scene& scene, Entity entity )
{
    return scene.HasEntity( entity ) and scene.HasComponent<TagComponent>( entity )
           ? scene.GetComponent<TagComponent>( entity ).mName : string();
}

namespace
{
string ValidName( string_view wanted )
{
    string name;
    for ( const char c : wanted )
        if ( c != '/' )
            name += c;
    // What a path would read as something else is not a name.
    if ( name.empty() or name == "." or name == ".." or name == "~" )
        name = "Entity";
    return name;
}

// "chair12" -> { "chair", 12 }; "chair" -> { "chair", 1 }.
std::pair<string_view, u64> SplitNumber( string_view name )
{
    size_t digits = name.size();
    while ( digits > 0 and std::isdigit( (unsigned char)name[digits - 1] ) )
        digits--;
    // All digits, or a number too long to count: the whole name is the base.
    if ( digits == 0 or name.size() - digits > 9 )
        return { name, 1 };
    if ( digits == name.size() )
        return { name, 1 };
    return { name.substr( 0, digits ), std::stoull( string( name.substr( digits ) ) ) };
}
}

string UniqueChildName( const Scene& scene, Entity parent, string_view wanted, Entity self )
{
    const string name = ValidName( wanted );
    str_hash_set taken;
    for ( const Entity sibling : ChildrenOf( scene, parent ) )
        if ( sibling != self and scene.HasComponent<TagComponent>( sibling ) )
            taken.insert( scene.GetComponent<TagComponent>( sibling ).mName );
    if ( not taken.contains( name ) )
        return name;

    const auto [base, number] = SplitNumber( name );
    for ( u64 n = std::max<u64>( number, 1 ) + 1;; n++ )
    {
        string candidate = std::format( "{}{}", base, n );
        if ( not taken.contains( candidate ) )
            return candidate;
    }
}

bool MakeNameUnique( Scene& scene, Entity entity )
{
    if ( not scene.HasEntity( entity ) or not scene.HasComponent<TagComponent>( entity ) )
        return false;
    const Entity parent = ParentOf( scene, entity );
    auto& tag = scene.GetComponent<TagComponent>( entity );
    // The root has no siblings; its name only has to be a valid one.
    string name = parent == INVALID_ENTITY ? ValidName( tag.mName )
                                           : UniqueChildName( scene, parent, tag.mName, entity );
    if ( name == tag.mName )
        return false;
    scene.GetComponent<TagComponent>( entity ).mName = std::move( name );
    return true;
}


void MakeNamesUnique( Scene& scene, Entity top )
{
    for ( const Entity entity : Subtree( scene, top ) )
    {
        // Siblings in order, each checked against the ones before it only,
        // so the first of two keeps its name.
        const vector<Entity> children( ChildrenOf( scene, entity ).begin(), ChildrenOf( scene, entity ).end() );
        str_hash_set taken;
        for ( const Entity child : children )
        {
            if ( not scene.HasComponent<TagComponent>( child ) )
                continue;
            string& name = scene.GetComponent<TagComponent>( child ).mName;
            string fixed = ValidName( name );
            if ( taken.contains( fixed ) )
            {
                const auto [base, number] = SplitNumber( fixed );
                for ( u64 n = std::max<u64>( number, 1 ) + 1;; n++ )
                {
                    string candidate = std::format( "{}{}", base, n );
                    // Not a name a later sibling has either, or that one
                    // would be renamed in turn for no reason.
                    const bool later = std::ranges::any_of( children, [&]( Entity other )
                    {
                        return other != child and scene.HasComponent<TagComponent>( other ) and
                               scene.GetComponent<TagComponent>( other ).mName == candidate;
                    } );
                    if ( not taken.contains( candidate ) and not later )
                    {
                        fixed = std::move( candidate );
                        break;
                    }
                }
            }
            if ( fixed != name )
                name = fixed;
            taken.insert( fixed );
        }
    }
}

Entity PrefabRootOf( const Scene& scene, Entity entity )
{
    int depth = 0;
    for ( Entity at = entity; at != INVALID_ENTITY and scene.HasEntity( at ) and depth < cMaxDepth;
          at = ParentOf( scene, at ), depth++ )
        if ( scene.HasComponent<PrefabInstanceComponent>( at ) )
            return at;
    return scene.HasEntity( entity ) ? scene.Root() : INVALID_ENTITY;
}

namespace
{
// Walks `path` from `from`. On the way to nothing, `why` (when given) says
// where it stopped and what was there instead.
Entity WalkPath( const Scene& scene, Entity from, string_view path, string* why )
{
    const auto fail = [&]( string reason )
    {
        if ( why )
            *why = std::move( reason );
        return INVALID_ENTITY;
    };
    if ( from == INVALID_ENTITY or not scene.HasEntity( from ) )
        return fail( std::format( "{} is not in the scene", DescribeEntity( scene, from ) ) );

    Entity at = from;
    if ( path.starts_with( '/' ) )
    {
        at = scene.Root();
        path.remove_prefix( 1 );
    }
    else if ( path == "~" or path.starts_with( "~/" ) )
    {
        at = PrefabRootOf( scene, from );
        path.remove_prefix( 1 );
    }
    while ( not path.empty() )
    {
        const size_t slash = path.find( '/' );
        const string_view part = path.substr( 0, slash );
        path = slash == string_view::npos ? string_view() : path.substr( slash + 1 );
        if ( part.empty() or part == "." )
            continue;
        if ( part == ".." )
        {
            const Entity parent = ParentOf( scene, at );
            if ( parent == INVALID_ENTITY )
                return fail( std::format( "{} has no parent", DescribeEntity( scene, at ) ) );
            at = parent;
            continue;
        }

        Entity found = INVALID_ENTITY;
        for ( const Entity child : ChildrenOf( scene, at ) )
            if ( scene.HasComponent<TagComponent>( child ) and scene.GetComponent<TagComponent>( child ).mName == part )
            {
                found = child;
                break;
            }
        if ( found == INVALID_ENTITY )
        {
            if ( not why )
                return INVALID_ENTITY;
            // What is there, and whether one of those is what was meant.
            string names, alike;
            int listed = 0;
            for ( const Entity child : ChildrenOf( scene, at ) )
            {
                const string name = NameOf( scene, child );
                if ( name.empty() )
                    continue;
                if ( listed++ < 12 )
                    names += std::format( "{}'{}'", names.empty() ? "" : ", ", name );
                if ( alike.empty() and LooksAlike( name, part ) )
                {
                    const string foreign = NonAsciiLetters( name ), asked = NonAsciiLetters( part );
                    alike = std::format( " '{}' looks like it but is spelled differently", name );
                    if ( not foreign.empty() )
                        alike += std::format( " - its letters {} are not Latin", foreign );
                    if ( not asked.empty() )
                        alike += std::format( " - the path's letters {} are not Latin", asked );
                    alike += ".";
                }
            }
            if ( listed > 12 )
                names += std::format( " and {} more", listed - 12 );
            return fail( std::format( "{} has no child '{}'. {}{}", DescribeEntity( scene, at ), part,
                                      names.empty() ? "It has no children." : std::format( "Its children: {}.", names ),
                                      alike ) );
        }
        at = found;
    }
    return at;
}
}

Entity FindByPath( const Scene& scene, Entity from, string_view path )
{
    return WalkPath( scene, from, path, nullptr );
}

string WhyPathFails( const Scene& scene, Entity from, string_view path )
{
    string why;
    if ( WalkPath( scene, from, path, &why ) != INVALID_ENTITY )
        return {};
    return why;
}

namespace
{
// The entity and its ancestors, the root last; empty when it does not reach
// the root.
vector<Entity> ChainToRoot( const Scene& scene, Entity entity )
{
    vector<Entity> chain;
    for ( Entity at = entity; at != INVALID_ENTITY and scene.HasEntity( at ); at = ParentOf( scene, at ) )
    {
        chain.push_back( at );
        if ( chain.size() > cMaxDepth )
            return {};
    }
    if ( chain.empty() or chain.back() != scene.Root() )
        return {};
    return chain;
}
}

string PathOf( const Scene& scene, Entity entity )
{
    const vector<Entity> chain = ChainToRoot( scene, entity );
    if ( chain.empty() )
        return {};
    if ( chain.size() == 1 )
        return "/";
    string path;
    for ( auto it = chain.rbegin() + 1; it != chain.rend(); ++it )
        path += "/" + NameOf( scene, *it );
    return path;
}

string RelativePath( const Scene& scene, Entity from, Entity to )
{
    const vector<Entity> up = ChainToRoot( scene, from );
    const vector<Entity> down = ChainToRoot( scene, to );
    if ( up.empty() or down.empty() )
        return {};
    // Strip the shared part, from the root end.
    size_t shared = 0;
    while ( shared < up.size() and shared < down.size() and
            up[up.size() - 1 - shared] == down[down.size() - 1 - shared] )
        shared++;
    string path;
    for ( size_t i = 0; i < up.size() - shared; i++ )
        path += path.empty() ? ".." : "/..";
    for ( size_t i = down.size() - shared; i-- > 0; )
        path += ( path.empty() ? "" : "/" ) + NameOf( scene, down[i] );
    return path.empty() ? "." : path;
}

string DescribeEntity( const Scene& scene, Entity entity )
{
    if ( entity == INVALID_ENTITY or not scene.HasEntity( entity ) )
        return std::format( "entity {} (removed)", (u64)entity );
    const string path = PathOf( scene, entity );
    if ( not path.empty() )
        return std::format( "'{}'", path );
    const string name = NameOf( scene, entity );
    return name.empty() ? std::format( "entity {} (no name, not in the tree)", (u64)entity )
                        : std::format( "'{}' (entity {}, not in the tree)", name, (u64)entity );
}

}
