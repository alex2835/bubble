#include "bubble/scene/hierarchy.hpp"
#include "bubble/scene/scene.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
namespace
{
// A message lists this many children of a parent, then counts the rest.
constexpr size_t cListedChildren = 12;

Hierarchy& NodeOf( Scene& scene, Entity entity, string_view what )
{
    const auto node = scene.Get<Hierarchy>( entity );
    if ( not node )
        throw logic_error( format( "{} on {}", what, Describe( scene, entity ) ) );
    return *node;
}

Text Fit( const Text& wanted )
{
    string name = wanted.String();
    ranges::replace( name, '/', '_' );
    if ( name.empty() or name == "." or name == ".." )
        name = "entity";
    return *Text::From( name );
}

// `wanted` made fit and free among `parent`'s children other than `self`:
// chair, then chair_2, chair_3... and chair_2 goes on to chair_3.
Text UniqueName( const Hierarchy& parent, const Text& wanted, Entity self )
{
    const Text name = Fit( wanted );
    const auto taken = [&]( string_view candidate ) {
        const auto found = parent.mChildByName.find( candidate );
        return found != parent.mChildByName.end() and found->second != self;
    };
    if ( not taken( name.View() ) )
        return name;

    string_view base = name.View();
    u32 number = 1;
    if ( const size_t underscore = base.rfind( '_' ); underscore != string_view::npos )
    {
        const string_view digits = base.substr( underscore + 1 );
        const auto parsed = TryParse<u32>( digits );
        if ( parsed and *parsed >= 2 and digits.front() != '0' )
        {
            number = *parsed;
            base = base.substr( 0, underscore );
        }
    }
    for ( u32 next = number + 1;; ++next )
    {
        string candidate = format( "{}_{}", base, next );
        if ( not taken( candidate ) )
            return *Text::From( candidate );
    }
}

void Link( Hierarchy& parent, Entity child, const Text& name, size_t index )
{
    parent.mChildren.insert( parent.mChildren.begin() + min( index, parent.mChildren.size() ), child );
    parent.mChildByName.emplace( name.String(), child );
}

void Unlink( Hierarchy& parent, Entity child, const Text& name )
{
    erase( parent.mChildren, child );
    parent.mChildByName.erase( parent.mChildByName.find( name.View() ) );
}

string ChildList( const Scene& scene, const Hierarchy& node )
{
    if ( node.mChildren.empty() )
        return "it has no children";
    string names;
    for ( size_t i = 0; i < node.mChildren.size() and i < cListedChildren; ++i )
        names += format( "{}'{}'", names.empty() ? "" : ", ", NameOf( scene, node.mChildren[i] ).View() );
    if ( node.mChildren.size() > cListedChildren )
        names += format( " and {} more", node.mChildren.size() - cListedChildren );
    return format( "children: {}", names );
}

// The entity and its ancestors, the root last; empty outside the tree.
vector<Entity> ChainToRoot( const Scene& scene, Entity entity )
{
    vector<Entity> chain;
    if ( not InTree( scene, entity ) )
        return chain;
    for ( Entity at = entity; at != Entity::Null; at = ParentOf( scene, at ) )
        chain.push_back( at );
    return chain;
}
}

Entity CreateChild( Scene& scene, Entity parent, const Text& name, size_t index )
{
    // Good across the Add below: adding never moves a component.
    Hierarchy& parentNode = NodeOf( scene, parent, "CreateChild" );
    const Text unique = UniqueName( parentNode, name, Entity::Null );
    const Entity child = scene.Create();
    Hierarchy& node = scene.Add<Hierarchy>( child );
    node.mName = unique;
    node.mParent = parent;
    Link( parentNode, child, unique, index );
    return child;
}

Entity ParentOf( const Scene& scene, Entity entity )
{
    const auto node = scene.Get<Hierarchy>( entity );
    return node ? node->mParent : Entity::Null;
}

span<const Entity> ChildrenOf( const Scene& scene, Entity entity )
{
    const auto node = scene.Get<Hierarchy>( entity );
    return node ? span<const Entity>( node->mChildren ) : span<const Entity>();
}

Text NameOf( const Scene& scene, Entity entity )
{
    const auto node = scene.Get<Hierarchy>( entity );
    return node ? node->mName : Text();
}

bool InTree( const Scene& scene, Entity entity )
{
    return scene.Has<Hierarchy>( entity );
}

bool IsAncestor( const Scene& scene, Entity ancestor, Entity entity )
{
    for ( Entity at = ParentOf( scene, entity ); at != Entity::Null; at = ParentOf( scene, at ) )
        if ( at == ancestor )
            return true;
    return false;
}

Text Rename( Scene& scene, Entity entity, const Text& wanted )
{
    Hierarchy& node = NodeOf( scene, entity, "Rename" );
    if ( node.mParent == Entity::Null )
        throw logic_error( "Rename on the root, which has no name" );
    Hierarchy& parent = *scene.Get<Hierarchy>( node.mParent );
    Text unique = UniqueName( parent, wanted, entity );
    if ( unique != node.mName )
    {
        // Its place among the siblings stays; only the index by name changes.
        parent.mChildByName.erase( parent.mChildByName.find( node.mName.View() ) );
        parent.mChildByName.emplace( unique.String(), entity );
        node.mName = unique;
    }
    return unique;
}

expected<void, string> SetParent( Scene& scene, Entity child, Entity parent, size_t index )
{
    const auto refuse = [&]( string_view why ) {
        return unexpected(
            format( "{} cannot go under {}: {}", Describe( scene, child ), Describe( scene, parent ), why ) );
    };
    if ( not InTree( scene, child ) )
        return refuse( "it is not in the tree" );
    if ( not InTree( scene, parent ) )
        return refuse( "that is not in the tree" );
    if ( child == scene.Root() )
        return refuse( "the root stays the root" );
    if ( parent == child or IsAncestor( scene, child, parent ) )
        return refuse( "that is inside it" );

    Hierarchy& node = *scene.Get<Hierarchy>( child );
    Unlink( *scene.Get<Hierarchy>( node.mParent ), child, node.mName );
    Hierarchy& newParent = *scene.Get<Hierarchy>( parent );
    node.mName = UniqueName( newParent, node.mName, child );
    node.mParent = parent;
    Link( newParent, child, node.mName, index );
    return {};
}

EntityPath PathOf( const Scene& scene, Entity entity )
{
    const vector<Entity> chain = ChainToRoot( scene, entity );
    if ( chain.empty() )
        return {};
    if ( chain.size() == 1 )
        return EntityPath{ *Text::From( "/" ) };
    string path;
    for ( auto at = chain.rbegin() + 1; at != chain.rend(); ++at )
        path += "/" + NameOf( scene, *at ).String();
    return EntityPath{ *Text::From( path ) };
}

EntityPath RelativePath( const Scene& scene, Entity from, Entity to )
{
    const vector<Entity> up = ChainToRoot( scene, from );
    const vector<Entity> down = ChainToRoot( scene, to );
    if ( up.empty() or down.empty() )
        return {};
    // What the two chains share, counted from the root.
    size_t shared = 0;
    while ( shared < up.size() and shared < down.size() and
            up[up.size() - 1 - shared] == down[down.size() - 1 - shared] )
        ++shared;
    string path;
    for ( size_t i = 0; i < up.size() - shared; ++i )
        path += path.empty() ? ".." : "/..";
    for ( size_t i = down.size() - shared; i-- > 0; )
        path += ( path.empty() ? "" : "/" ) + NameOf( scene, down[i] ).String();
    return EntityPath{ *Text::From( path.empty() ? "." : path ) };
}

expected<Entity, string> Resolve( const Scene& scene, const EntityPath& path, Entity from )
{
    string_view rest = path.mPath.View();
    const auto fail = [&]( string_view why ) { return unexpected( format( "{}: {}", path.mPath.View(), why ) ); };
    if ( rest.empty() )
        return unexpected( "an empty path"s );

    Entity at = from;
    if ( rest.starts_with( '/' ) )
    {
        at = scene.Root();
        rest.remove_prefix( 1 );
        if ( rest.empty() )
            return at;
    }
    else if ( not InTree( scene, from ) )
        return fail( format( "a relative path, from {}", Describe( scene, from ) ) );

    for ( ;; )
    {
        const size_t slash = rest.find( '/' );
        const string_view step = rest.substr( 0, slash );
        if ( step.empty() )
            return fail( "an empty name between slashes" );
        if ( step == ".." )
        {
            if ( ParentOf( scene, at ) == Entity::Null )
                return fail( format( "{} has no parent", Describe( scene, at ) ) );
            at = ParentOf( scene, at );
        }
        else if ( step != "." )
        {
            const Hierarchy& node = *scene.Get<Hierarchy>( at );
            const auto found = node.mChildByName.find( step );
            if ( found == node.mChildByName.end() )
                return fail(
                    format( "{} has no child '{}' ({})", Describe( scene, at ), step, ChildList( scene, node ) ) );
            at = found->second;
        }
        if ( slash == string_view::npos )
            return at;
        rest.remove_prefix( slash + 1 );
    }
}

string Describe( const Scene& scene, Entity entity )
{
    if ( entity == Entity::Null )
        return "no entity";
    if ( not scene.Alive( entity ) )
        return format( "entity {} (gone)", static_cast<u32>( entity ) );
    if ( not InTree( scene, entity ) )
        return format( "entity {} (not in the tree)", static_cast<u32>( entity ) );
    return string( PathOf( scene, entity ).mPath.View() );
}
}
