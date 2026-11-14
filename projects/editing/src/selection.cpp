#include "bubble/editing/selection.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
namespace
{
// Where an entity is in the tree, as the place among its siblings at each
// level from the root: comparing two puts them in the tree's order.
vector<size_t> PlaceInTree( const Scene& scene, Entity entity )
{
    vector<size_t> place;
    for ( Entity at = entity; ParentOf( scene, at ) != Entity::Null; at = ParentOf( scene, at ) )
    {
        const span<const Entity> siblings = ChildrenOf( scene, ParentOf( scene, at ) );
        place.push_back( static_cast<size_t>( ranges::find( siblings, at ) - siblings.begin() ) );
    }
    ranges::reverse( place );
    return place;
}
}

vector<Entity> TopOf( const Scene& scene, span<const Entity> entities )
{
    vector<pair<vector<size_t>, Entity>> placed;
    for ( const Entity entity : entities )
        if ( ranges::none_of( entities, [&]( Entity other ) { return IsAncestor( scene, other, entity ); } ) and
             ranges::find( placed, entity, &pair<vector<size_t>, Entity>::second ) == placed.end() )
            placed.emplace_back( PlaceInTree( scene, entity ), entity );
    ranges::sort( placed, []( const auto& a, const auto& b ) { return a.first < b.first; } );
    vector<Entity> top;
    for ( const auto& [place, entity] : placed )
        top.push_back( entity );
    return top;
}

void Selection::Select( Entity entity )
{
    Require( entity, "Select" );
    mPicked.assign( 1, entity );
}

void Selection::Add( Entity entity )
{
    Require( entity, "Add" );
    erase( mPicked, entity );
    mPicked.push_back( entity );
}

void Selection::Remove( Entity entity )
{
    Require( entity, "Remove" );
    erase( mPicked, entity );
}

void Selection::Toggle( Entity entity )
{
    if ( Has( entity ) )
        Remove( entity );
    else
        Add( entity );
}

void Selection::Clear()
{
    mPicked.clear();
}

bool Selection::Has( Entity entity ) const
{
    return Shown( entity ) and ranges::find( mPicked, entity ) != mPicked.end();
}

vector<Entity> Selection::All() const
{
    vector<Entity> shown;
    for ( const Entity entity : mPicked )
        if ( Shown( entity ) )
            shown.push_back( entity );
    return shown;
}

Entity Selection::Active() const
{
    for ( auto entity = mPicked.rbegin(); entity != mPicked.rend(); ++entity )
        if ( Shown( *entity ) )
            return *entity;
    return Entity::Null;
}

vector<Entity> Selection::TopLevel() const
{
    return TopOf( mScene, All() );
}

bool Selection::Shown( Entity entity ) const
{
    return InTree( mScene, entity );
}

void Selection::Require( Entity entity, string_view what ) const
{
    if ( not InTree( mScene, entity ) )
        throw logic_error(
            format( "Selection::{} of {}: only an entity of the tree is selected", what, Describe( mScene, entity ) ) );
}
}
