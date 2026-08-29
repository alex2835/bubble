#include "recs/registry.hpp"

#include <cassert>
#include <iostream>
#include <string_view>
#include <stdexcept>
#include <memory>
#include <vector>
#include <array>
#include <algorithm>
#include <utility>

// ============================ component fixtures ============================

enum class Component
{
    Speed,
    Position,
    Health
};

struct Speed
{
    int s = 0;

    static int ID() { return (int)Component::Speed; }
    bool operator==( const Speed& ) const = default;
};

struct Position
{
    int x = 0;
    int y = 0;

    static int ID() { return (int)Component::Position; }
    bool operator==( const Position& ) const = default;
};

struct Health
{
    int hp = 100;

    static int ID() { return (int)Component::Health; }
    bool operator==( const Health& ) const = default;
};

// =============================== test helpers ===============================

// n entities with no components, followed by n entities carrying Speed( 0..n-1 ).
// Returns the component-less ones, whose ids sort before all the others.
static std::vector<recs::Entity> BuildSplitRegistry( recs::Registry& registry, int n )
{
    std::vector<recs::Entity> bare;
    for ( int i = 0; i < n; i++ )
        bare.push_back( registry.CreateEntity() );

    for ( int i = 0; i < n; i++ )
        registry.AddComponent<Speed>( registry.CreateEntity(), i );

    return bare;
}

static int CountSpeed( recs::Registry& registry )
{
    int count = 0;
    registry.ForEach<Speed>( [&]( recs::Entity, Speed& ) { count++; } );
    return count;
}

static int CountPosition( recs::Registry& registry )
{
    int count = 0;
    registry.ForEach<Position>( [&]( recs::Entity, Position& ) { count++; } );
    return count;
}

template <typename Fn>
static bool Throws( Fn&& fn )
{
    try { fn(); }
    catch ( const std::exception& ) { return true; }
    return false;
}

// ========================= AddComponent / HasComponent ======================

void test_add_component_then_has_component()
{
    recs::Registry registry;
    recs::Entity entity = registry.CreateEntity();

    registry.AddComponent<Speed>( entity, 5 );
    assert( registry.HasComponent<Speed>( entity ) );
}

void test_has_component_false_for_other_type()
{
    recs::Registry registry;
    recs::Entity entity = registry.CreateEntity();

    registry.AddComponent<Speed>( entity, 5 );
    assert( !registry.HasComponent<Position>( entity ) );
}

void test_has_component_false_for_unregistered_type()
{
    recs::Registry registry;
    recs::Entity entity = registry.CreateEntity();
    assert( !registry.HasComponent<Speed>( entity ) );
}

void test_has_components_false_for_unregistered_types()
{
    recs::Registry registry;
    recs::Entity entity = registry.CreateEntity();
    assert( ( !registry.HasComponents<Speed, Position>( entity ) ) );
}

void test_add_component_stores_constructor_arguments()
{
    recs::Registry registry;
    recs::Entity entity = registry.CreateEntity();

    registry.AddComponent<Position>( entity, 3, 7 );

    const Position& position = registry.GetComponent<Position>( entity );
    assert( position.x == 3 );
    assert( position.y == 7 );
}

void test_register_type_creates_no_entity()
{
    recs::Registry registry;
    registry.AddComponent<Speed>();

    assert( registry.Size() == 0 );
    assert( registry.AllComponentTypeIds().contains( Speed::ID() ) );
}

// ============================== GetComponent ================================

void test_get_component_returns_live_reference()
{
    recs::Registry registry;
    recs::Entity entity = registry.CreateEntity();
    registry.AddComponent<Speed>( entity, 5 );

    Speed& speed = registry.GetComponent<Speed>( entity );
    speed.s += 1;

    // The write must be visible through an independent lookup.
    assert( registry.GetComponent<Speed>( entity ).s == 6 );
}

void test_get_component_is_stable_across_calls()
{
    recs::Registry registry;
    recs::Entity entity = registry.CreateEntity();
    registry.AddComponent<Speed>( entity, 5 );

    Speed& first = registry.GetComponent<Speed>( entity );
    Speed& second = registry.GetComponent<Speed>( entity );
    assert( &first == &second );
}

void test_get_component_throws_when_absent()
{
    recs::Registry registry;
    recs::Entity entity = registry.CreateEntity();
    registry.AddComponent<Speed>( entity, 1 );
    registry.AddComponent<Position>();

    assert( Throws( [&] { registry.GetComponent<Position>( entity ); } ) );
}

// ============================= invalid entity ===============================

void test_has_component_rejects_invalid_entity()
{
    recs::Registry registry;
    registry.AddComponent<Speed>();
    assert( Throws( [&] { registry.HasComponent<Speed>( recs::INVALID_ENTITY ); } ) );
}

void test_get_component_rejects_invalid_entity()
{
    recs::Registry registry;
    registry.AddComponent<Speed>();
    assert( Throws( [&] { registry.GetComponent<Speed>( recs::INVALID_ENTITY ); } ) );
}

void test_add_component_rejects_invalid_entity()
{
    recs::Registry registry;
    registry.AddComponent<Speed>();
    assert( Throws( [&] { registry.AddComponent<Speed>( recs::INVALID_ENTITY ); } ) );
}

void test_create_entity_with_zero_id_rejected()
{
    recs::Registry registry;
    assert( Throws( [&] { registry.CreateEntityWithId( 0 ); } ) );
    assert( registry.Size() == 0 );
}

// ============================== duplicate add ===============================

void test_duplicate_add_updates_value()
{
    recs::Registry registry;
    recs::Entity entity = registry.CreateEntity();

    registry.AddComponent<Speed>( entity, 10 );
    assert( registry.GetComponent<Speed>( entity ).s == 10 );

    registry.AddComponent<Speed>( entity, 99 );
    assert( registry.GetComponent<Speed>( entity ).s == 99 );
}

void test_duplicate_add_creates_no_second_entry()
{
    recs::Registry registry;
    recs::Entity entity = registry.CreateEntity();

    registry.AddComponent<Speed>( entity, 10 );
    registry.AddComponent<Speed>( entity, 99 );

    assert( CountSpeed( registry ) == 1 );
}

// ============================== GetComponents ===============================

void test_get_components_returns_live_references()
{
    recs::Registry registry;
    recs::Entity entity = registry.CreateEntity();
    registry.AddComponent<Speed>( entity, 1 );
    registry.AddComponent<Position>( entity, 3, 7 );

    auto [speed, position] = registry.GetComponents<Speed, Position>( entity );
    speed.s += 1;

    assert( speed == registry.GetComponent<Speed>( entity ) );
}

void test_get_components_returns_each_component()
{
    recs::Registry registry;
    recs::Entity entity = registry.CreateEntity();
    registry.AddComponent<Speed>( entity, 1 );
    registry.AddComponent<Position>( entity, 3, 7 );

    auto [speed, position] = registry.GetComponents<Speed, Position>( entity );
    assert( speed.s == 1 );
    assert( position.x == 3 );
    assert( position.y == 7 );
}

void test_get_components_throws_when_one_is_absent()
{
    recs::Registry registry;
    recs::Entity entity = registry.CreateEntity();
    registry.AddComponent<Speed>( entity, 1 );
    registry.AddComponent<Position>();

    assert( Throws( [&] { registry.GetComponents<Speed, Position>( entity ); } ) );
}

// =============================== HasComponents ==============================

void test_has_components_single_type()
{
    recs::Registry registry;
    recs::Entity entity = registry.CreateEntity();
    registry.AddComponent<Speed>( entity, 1 );

    assert( registry.HasComponents<Speed>( entity ) );
}

void test_has_components_false_when_one_missing()
{
    recs::Registry registry;
    recs::Entity entity = registry.CreateEntity();
    registry.AddComponent<Speed>( entity, 1 );

    assert( ( !registry.HasComponents<Speed, Position>( entity ) ) );
}

void test_has_components_true_when_all_present()
{
    recs::Registry registry;
    recs::Entity entity = registry.CreateEntity();
    registry.AddComponent<Speed>( entity, 1 );
    registry.AddComponent<Position>( entity, 0, 0 );

    assert( ( registry.HasComponents<Speed, Position>( entity ) ) );
}

// ============================== entity lookup ===============================

void test_get_entity_by_id_round_trips()
{
    recs::Registry registry;
    recs::Entity e1 = registry.CreateEntity();
    recs::Entity e2 = registry.CreateEntity();

    assert( registry.GetEntityById( (size_t)e1 ) == e1 );
    assert( registry.GetEntityById( (size_t)e2 ) == e2 );
}

void test_get_entity_by_unknown_id_returns_invalid()
{
    recs::Registry registry;
    registry.CreateEntity();
    assert( registry.GetEntityById( 9999 ) == recs::INVALID_ENTITY );
}

void test_create_entity_with_id_advances_counter()
{
    recs::Registry registry;
    registry.CreateEntityWithId( 500 );

    // A later generated id must not collide with the explicit one.
    recs::Entity next = registry.CreateEntity();
    assert( (size_t)next > 500 );
}

// ================================= ForEach ==================================

void test_foreach_unregistered_type_yields_nothing()
{
    recs::Registry registry;
    assert( CountSpeed( registry ) == 0 );
}

void test_foreach_visits_every_entity_with_component()
{
    recs::Registry registry;
    BuildSplitRegistry( registry, 10 );
    assert( CountSpeed( registry ) == 10 );
}

void test_foreach_two_components_yields_intersection()
{
    recs::Registry registry;
    std::vector<recs::Entity> bare = BuildSplitRegistry( registry, 10 );

    int i = 0;
    for ( recs::Entity entity : bare )
    {
        registry.AddComponent<Speed>( entity, 1 );
        registry.AddComponent<Position>( entity, i++, 1 );
    }

    int count = 0;
    registry.ForEach<Speed, Position>( [&]( recs::Entity, Speed&, Position& ) { count++; } );
    assert( count == 10 );

    // ...while the single-component query still sees all 20.
    assert( CountSpeed( registry ) == 20 );
}

void test_foreach_no_overlap_yields_nothing()
{
    recs::Registry registry;

    recs::Entity e1 = registry.CreateEntity();
    recs::Entity e2 = registry.CreateEntity();
    registry.AddComponent<Speed>( e1, 1 );
    registry.AddComponent<Position>( e2, 2, 3 );

    int count = 0;
    registry.ForEach<Speed, Position>( [&]( recs::Entity, Speed&, Position& ) { count++; } );
    assert( count == 0 );
}

void test_foreach_interleaved_ids_yields_only_intersection()
{
    recs::Registry registry;

    recs::Entity e1 = registry.CreateEntity();
    recs::Entity e2 = registry.CreateEntity();
    recs::Entity e3 = registry.CreateEntity();

    // Speed pool: [e1, e3], Position pool: [e2, e3] - only e3 has both.
    registry.AddComponent<Speed>( e1, 10 );
    registry.AddComponent<Speed>( e3, 30 );
    registry.AddComponent<Position>( e2, 20, 0 );
    registry.AddComponent<Position>( e3, 30, 0 );

    int count = 0;
    registry.ForEach<Speed, Position>( [&]( recs::Entity, Speed& s, Position& p )
    {
        count++;
        assert( s.s == 30 );
        assert( p.x == 30 );
    } );
    assert( count == 1 );
}

void test_foreach_three_components_yields_intersection()
{
    recs::Registry registry;

    recs::Entity e1 = registry.CreateEntity();
    registry.AddComponent<Speed>( e1, 1 );
    registry.AddComponent<Position>( e1, 2, 3 );
    registry.AddComponent<Health>( e1, 50 );

    recs::Entity e2 = registry.CreateEntity();
    registry.AddComponent<Speed>( e2, 10 );
    registry.AddComponent<Position>( e2, 20, 30 );
    // e2 has no Health

    int count = 0;
    registry.ForEach<Speed, Position, Health>( [&]( recs::Entity, Speed& s, Position& p, Health& h )
    {
        count++;
        assert( s.s == 1 );
        assert( p.x == 2 );
        assert( h.hp == 50 );
    } );
    assert( count == 1 );
}

void test_foreach_large_population_single_component()
{
    recs::Registry registry;
    constexpr int N = 1000;

    for ( int i = 0; i < N; i++ )
        registry.AddComponent<Speed>( registry.CreateEntity(), i );

    assert( CountSpeed( registry ) == N );
}

void test_foreach_large_population_two_component_join()
{
    recs::Registry registry;
    constexpr int N = 1000;

    for ( int i = 0; i < N; i++ )
    {
        recs::Entity e = registry.CreateEntity();
        registry.AddComponent<Speed>( e, i );
        if ( i % 2 == 0 )
            registry.AddComponent<Position>( e, i, i );
    }

    int count = 0;
    registry.ForEach<Speed, Position>( [&]( recs::Entity, Speed& s, Position& p )
    {
        assert( s.s == p.x );
        count++;
    } );
    assert( count == N / 2 );
}

void test_foreach_functor_not_consumed()
{
    recs::Registry registry;
    for ( int i = 0; i < 5; i++ )
        registry.AddComponent<Speed>( registry.CreateEntity(), i );

    struct CountingFunctor
    {
        std::shared_ptr<int> calls = std::make_shared<int>( 0 );
        void operator()( recs::Entity, Speed& )
        {
            assert( calls && "functor was moved-from mid-iteration" );
            ( *calls )++;
        }
    };

    CountingFunctor functor;
    auto calls = functor.calls;
    registry.ForEach<Speed>( std::move( functor ) );
    assert( *calls == 5 );
}

// ================================== View ====================================

void test_view_unregistered_type_is_empty()
{
    recs::Registry registry;
    auto view = registry.GetView<Speed>();
    assert( view.Size() == 0 );
}

void test_view_iterates_in_entity_id_order()
{
    recs::Registry registry;
    BuildSplitRegistry( registry, 10 );

    int i = 0;
    for ( auto&& [speed] : registry.GetView<Speed>() )
        assert( speed.s == i++ );
    assert( i == 10 );
}

void test_view_sees_components_added_later()
{
    recs::Registry registry;
    std::vector<recs::Entity> bare = BuildSplitRegistry( registry, 10 );

    // These ids sort before the existing pool contents, so the pool has to
    // insert rather than append.
    for ( recs::Entity entity : bare )
        registry.AddComponent<Speed>( entity, 5 );

    for ( recs::Entity entity : bare )
        assert( registry.GetComponent<Speed>( entity ).s == 5 );

    int count = 0;
    for ( auto&& [speed] : registry.GetView<Speed>() )
    {
        assert( speed.s < 10 );
        count++;
    }
    assert( count == 20 );
}

void test_view_matches_foreach()
{
    recs::Registry registry;

    for ( int i = 0; i < 5; i++ )
    {
        recs::Entity e = registry.CreateEntity();
        registry.AddComponent<Speed>( e, i * 10 );
        registry.AddComponent<Position>( e, i, i );
    }
    for ( int i = 0; i < 5; i++ )
        registry.AddComponent<Speed>( registry.CreateEntity(), i * 100 ); // no Position

    std::vector<std::pair<int, int>> fromForEach;
    registry.ForEach<Speed, Position>( [&]( recs::Entity, Speed& s, Position& p )
    {
        fromForEach.emplace_back( s.s, p.x );
    } );

    std::vector<std::pair<int, int>> fromView;
    for ( auto&& [s, p] : registry.GetView<Speed, Position>() )
        fromView.emplace_back( s.s, p.x );

    assert( fromForEach == fromView );
}

// Regression: the merge-join gallops over gaps. Check it lands on the same
// intersection a linear scan would, including boundary and adjacent entries.
void test_sparse_join_finds_all_matches()
{
    recs::Registry registry;

    std::vector<recs::Entity> all;
    for ( int i = 0; i < 500; i++ )
    {
        recs::Entity e = registry.CreateEntity();
        registry.AddComponent<Speed>( e, i );
        all.push_back( e );
    }

    const int picks[] = { 0, 1, 137, 138, 300, 499 };
    for ( int idx : picks )
        registry.AddComponent<Position>( all[idx], idx, 0 );

    std::vector<int> seen;
    registry.ForEach<Position, Speed>( [&]( recs::Entity, Position& p, Speed& s )
    {
        assert( p.x == s.s );
        seen.push_back( p.x );
    } );

    assert( seen.size() == 6 );
    for ( size_t i = 0; i < seen.size(); i++ )
        assert( seen[i] == picks[i] );
}

void test_sparse_join_view_matches_foreach()
{
    recs::Registry registry;

    std::vector<recs::Entity> all;
    for ( int i = 0; i < 500; i++ )
    {
        recs::Entity e = registry.CreateEntity();
        registry.AddComponent<Speed>( e, i );
        all.push_back( e );
    }

    for ( int idx : { 0, 1, 137, 138, 300, 499 } )
        registry.AddComponent<Position>( all[idx], idx, 0 );

    std::vector<int> fromForEach;
    registry.ForEach<Position, Speed>( [&]( recs::Entity, Position& p, Speed& ) { fromForEach.push_back( p.x ); } );

    std::vector<int> fromView;
    for ( auto&& [p, s] : registry.GetView<Position, Speed>() )
        fromView.push_back( p.x );

    assert( fromForEach == fromView );
}

// ============================== RuntimeForEach ==============================

void test_runtime_foreach_all_invalid_terminates()
{
    recs::Registry registry;
    registry.AddComponent<Speed>();
    registry.AddComponent<Speed>( registry.CreateEntity(), 1 );

    std::array<recs::ComponentTypeId, 3> ids;
    ids.fill( recs::INVALID_COMPONENT_TYPE_ID );

    int count = 0;
    registry.RuntimeForEach( ids, [&]( recs::Entity, std::array<void*, 3> ) { count++; } );
    assert( count == 0 );
}

void test_runtime_foreach_mixed_ids_intersects()
{
    recs::Registry registry;

    recs::Entity both = registry.CreateEntity();
    registry.AddComponent<Speed>( both, 5 );
    registry.AddComponent<Position>( both, 1, 2 );

    recs::Entity speedOnly = registry.CreateEntity();
    registry.AddComponent<Speed>( speedOnly, 6 );

    std::array<recs::ComponentTypeId, 3> ids = {
        Speed::ID(), recs::INVALID_COMPONENT_TYPE_ID, Position::ID() };

    int count = 0;
    registry.RuntimeForEach( ids, [&]( recs::Entity e, std::array<void*, 3> data )
    {
        count++;
        assert( e == both );
        assert( data[1] == nullptr );
        assert( static_cast<Speed*>( data[0] )->s == 5 );
        assert( static_cast<Position*>( data[2] )->x == 1 );
    } );
    assert( count == 1 );
}

// ============================ component removal =============================

void test_remove_component_clears_has_component()
{
    recs::Registry registry;
    recs::Entity entity = registry.CreateEntity();
    registry.AddComponent<Position>( entity, 1, 1 );

    registry.RemoveComponent<Position>( entity );
    assert( !registry.HasComponent<Position>( entity ) );
}

void test_remove_component_clears_from_foreach()
{
    recs::Registry registry;
    std::vector<recs::Entity> entities;
    for ( int i = 0; i < 10; i++ )
    {
        recs::Entity e = registry.CreateEntity();
        registry.AddComponent<Position>( e, i, 1 );
        entities.push_back( e );
    }

    for ( recs::Entity entity : entities )
        registry.RemoveComponent<Position>( entity );

    assert( CountPosition( registry ) == 0 );
}

void test_remove_component_leaves_other_components()
{
    recs::Registry registry;
    recs::Entity entity = registry.CreateEntity();
    registry.AddComponent<Speed>( entity, 1 );
    registry.AddComponent<Position>( entity, 1, 1 );

    registry.RemoveComponent<Position>( entity );
    assert( registry.HasComponent<Speed>( entity ) );
}

void test_remove_missing_component_is_noop()
{
    recs::Registry registry;
    recs::Entity entity = registry.CreateEntity();
    registry.AddComponent<Speed>( entity, 1 );

    registry.RemoveComponent<Position>( entity );
    assert( registry.HasComponent<Speed>( entity ) );
    assert( !registry.HasComponent<Position>( entity ) );
}

// ============================== entity removal ==============================

void test_remove_entity_leaves_the_others()
{
    recs::Registry registry;
    std::vector<recs::Entity> bare = BuildSplitRegistry( registry, 10 );

    for ( recs::Entity entity : bare )
        registry.AddComponent<Speed>( entity, 1 );
    assert( CountSpeed( registry ) == 20 );

    for ( recs::Entity entity : bare )
        registry.RemoveEntity( entity );

    assert( CountSpeed( registry ) == 10 );
}

void test_remove_entity_drops_it_from_registry()
{
    recs::Registry registry;
    recs::Entity entity = registry.CreateEntity();
    assert( registry.Size() == 1 );

    registry.RemoveEntity( entity );
    assert( registry.Size() == 0 );
    assert( registry.GetEntityById( (size_t)entity ) == recs::INVALID_ENTITY );
}

// RemoveEntity looks the entity up rather than using operator[], so it cannot
// insert an entry on its way out. (Removing an entity the registry does not
// hold stays an assert - that is a caller bug, not a supported no-op.)
void test_remove_entity_does_not_fabricate_entries()
{
    recs::Registry registry;
    registry.CreateEntity();
    assert( registry.Size() == 1 );

    registry.RemoveEntity( registry.CreateEntity() );
    assert( registry.Size() == 1 );
}

void test_has_entity_true_for_live_entity()
{
    recs::Registry registry;
    recs::Entity entity = registry.CreateEntity();
    assert( registry.HasEntity( entity ) );
}

void test_has_entity_false_after_removal()
{
    recs::Registry registry;
    recs::Entity entity = registry.CreateEntity();
    registry.RemoveEntity( entity );
    assert( not registry.HasEntity( entity ) );
}

void test_has_entity_false_for_unknown_entity()
{
    recs::Registry registry;
    registry.CreateEntity();
    assert( not registry.HasEntity( registry.GetEntityById( 4242 ) ) );
    assert( not registry.HasEntity( recs::INVALID_ENTITY ) );
}

// An entity with no components is still an entity: HasEntity answers about the
// registry's entity set, not about any pool.
void test_has_entity_true_without_components()
{
    recs::Registry registry;
    registry.AddComponent<Speed>();
    recs::Entity entity = registry.CreateEntity();
    assert( not registry.HasComponent<Speed>( entity ) );
    assert( registry.HasEntity( entity ) );
}

// =========================== batch entity removal ===========================

void test_batch_remove_matches_individual_removal()
{
    auto build = []( recs::Registry& r, std::vector<recs::Entity>& es )
    {
        for ( int i = 0; i < 200; i++ )
        {
            recs::Entity e = r.CreateEntity();
            r.AddComponent<Speed>( e, i );
            if ( i % 3 == 0 )
                r.AddComponent<Position>( e, i, i );
            es.push_back( e );
        }
    };

    recs::Registry single, batch;
    std::vector<recs::Entity> a, b;
    build( single, a );
    build( batch, b );

    for ( size_t i = 0; i < a.size(); i += 2 )
        single.RemoveEntity( a[i] );

    std::vector<recs::Entity> victims;
    for ( size_t i = 0; i < b.size(); i += 2 )
        victims.push_back( b[i] );
    batch.RemoveEntities( victims );

    assert( single.Size() == batch.Size() );

    std::vector<std::pair<size_t, int>> fromSingle, fromBatch;
    single.ForEach<Speed>( [&]( recs::Entity e, Speed& s ) { fromSingle.emplace_back( (size_t)e, s.s ); } );
    batch.ForEach<Speed>( [&]( recs::Entity e, Speed& s ) { fromBatch.emplace_back( (size_t)e, s.s ); } );
    assert( fromSingle == fromBatch );

    std::vector<std::pair<size_t, int>> posSingle, posBatch;
    single.ForEach<Position>( [&]( recs::Entity e, Position& p ) { posSingle.emplace_back( (size_t)e, p.x ); } );
    batch.ForEach<Position>( [&]( recs::Entity e, Position& p ) { posBatch.emplace_back( (size_t)e, p.x ); } );
    assert( posSingle == posBatch );
}

void test_batch_remove_survivors_stay_reachable()
{
    recs::Registry registry;
    std::vector<recs::Entity> entities;
    for ( int i = 0; i < 200; i++ )
    {
        recs::Entity e = registry.CreateEntity();
        registry.AddComponent<Speed>( e, i );
        entities.push_back( e );
    }

    std::vector<recs::Entity> victims;
    for ( size_t i = 0; i < entities.size(); i += 2 )
        victims.push_back( entities[i] );
    registry.RemoveEntities( victims );

    for ( size_t i = 1; i < entities.size(); i += 2 )
    {
        assert( registry.HasComponent<Speed>( entities[i] ) );
        assert( registry.GetComponent<Speed>( entities[i] ).s == (int)i );
    }
}

void test_batch_remove_accepts_unsorted_input_with_duplicates()
{
    recs::Registry registry;
    std::vector<recs::Entity> entities;
    for ( int i = 0; i < 50; i++ )
    {
        recs::Entity e = registry.CreateEntity();
        registry.AddComponent<Speed>( e, i );
        entities.push_back( e );
    }

    std::vector<recs::Entity> victims;
    for ( size_t i = 0; i < entities.size(); i += 2 )
        victims.push_back( entities[i] );
    std::reverse( victims.begin(), victims.end() );
    victims.push_back( victims.front() ); // duplicate

    registry.RemoveEntities( victims );
    assert( registry.Size() == 25 );
    assert( CountSpeed( registry ) == 25 );
}

void test_batch_remove_empty_list_is_noop()
{
    recs::Registry registry;
    recs::Entity e = registry.CreateEntity();
    registry.AddComponent<Speed>( e, 1 );

    registry.RemoveEntities( {} );
    assert( registry.Size() == 1 );
    assert( registry.HasComponent<Speed>( e ) );
}

void test_batch_remove_ignores_unknown_entities()
{
    recs::Registry registry;
    recs::Entity e = registry.CreateEntity();
    registry.AddComponent<Speed>( e, 1 );

    // Ids this registry never issued. Entity ids are per-registry, not global,
    // so these have to be picked well clear of the ones above.
    recs::Registry other;
    std::vector<recs::Entity> unknown = { other.CreateEntityWithId( 9998 ),
                                          other.CreateEntityWithId( 9999 ) };

    registry.RemoveEntities( unknown );
    assert( registry.Size() == 1 );
    assert( registry.HasComponent<Speed>( e ) );
}

// ========================= component id (runtime) API =======================

void test_entity_add_component_id_is_idempotent()
{
    recs::Registry registry;
    registry.AddComponent<Speed>();

    recs::Entity e = registry.CreateEntity();
    registry.EntityAddComponentId( e, Speed::ID() );
    registry.EntityAddComponentId( e, Speed::ID() );
    registry.EntityAddComponentId( e, Speed::ID() );

    assert( CountSpeed( registry ) == 1 );
}

void test_entity_remove_component_id_removes_once()
{
    recs::Registry registry;
    registry.AddComponent<Speed>();

    recs::Entity e = registry.CreateEntity();
    registry.EntityAddComponentId( e, Speed::ID() );
    registry.EntityRemoveComponentId( e, Speed::ID() );

    assert( !registry.HasComponent<Speed>( e ) );
    assert( CountSpeed( registry ) == 0 );
}

void test_entity_remove_component_id_twice_is_noop()
{
    recs::Registry registry;
    registry.AddComponent<Speed>();

    recs::Entity e = registry.CreateEntity();
    registry.EntityAddComponentId( e, Speed::ID() );
    registry.EntityRemoveComponentId( e, Speed::ID() );
    registry.EntityRemoveComponentId( e, Speed::ID() );

    assert( !registry.HasComponent<Speed>( e ) );
}

// ============================== entity copying ==============================

// Regression: PushEmpty can reallocate and shift the pool, so the source
// component address must be read after the new slot exists, not before.
void test_copy_entity_across_pool_realloc()
{
    recs::Registry registry;
    registry.AddComponent<Position>();

    std::vector<recs::Entity> entities;
    for ( int i = 0; i < 64; i++ )
    {
        recs::Entity e = registry.CreateEntity();
        registry.AddComponent<Position>( e, i, i * 2 );
        entities.push_back( e );
    }

    for ( recs::Entity source : entities )
    {
        const Position expected = registry.GetComponent<Position>( source );
        recs::Entity copy = registry.CopyEntity( source );

        assert( registry.GetComponent<Position>( copy ) == expected );
        assert( registry.GetComponent<Position>( source ) == expected );
    }
}

void test_copy_entity_is_independent_of_source()
{
    recs::Registry registry;
    recs::Entity source = registry.CreateEntity();
    registry.AddComponent<Speed>( source, 7 );

    recs::Entity copy = registry.CopyEntity( source );
    registry.GetComponent<Speed>( copy ).s = 99;

    assert( registry.GetComponent<Speed>( source ).s == 7 );
}

// =============================== registry copy ==============================

void test_registry_copy_preserves_data()
{
    recs::Registry registry;
    std::vector<recs::Entity> bare = BuildSplitRegistry( registry, 10 );

    int i = 0;
    for ( recs::Entity entity : bare )
    {
        registry.AddComponent<Speed>( entity, 1 );
        registry.AddComponent<Position>( entity, i++, 1 );
    }

    std::vector<std::tuple<Speed, Position>> original;
    registry.ForEach<Speed, Position>( [&]( recs::Entity, Speed& s, Position& p )
    {
        original.emplace_back( s, p );
    } );
    assert( original.size() == 10 );

    recs::Registry copy = registry;
    std::vector<std::tuple<Speed, Position>> copied;
    copy.ForEach<Speed, Position>( [&]( recs::Entity, Speed& s, Position& p )
    {
        copied.emplace_back( s, p );
    } );

    assert( original == copied );
}

void test_registry_copy_is_isolated_from_original()
{
    recs::Registry registry;
    BuildSplitRegistry( registry, 10 );

    recs::Registry copy = registry;
    copy.ForEach<Speed>( [&]( recs::Entity, Speed& s ) { s.s = 999; } );
    registry.ForEach<Speed>( [&]( recs::Entity, Speed& s ) { assert( s.s != 999 ); } );
}

void test_registry_copy_preserves_entity_count()
{
    recs::Registry registry;
    std::vector<recs::Entity> bare = BuildSplitRegistry( registry, 10 );
    for ( recs::Entity entity : bare )
        registry.AddComponent<Speed>( entity, 1 );

    recs::Registry copy = registry;
    assert( CountSpeed( copy ) == 20 );
    assert( copy.Size() == registry.Size() );
}

// Regression: copy-assigning over a non-empty registry must destroy what the
// destination held rather than leaking it.
void test_registry_copy_assign_replaces_destination()
{
    recs::Registry source;
    for ( int i = 0; i < 3; i++ )
        source.AddComponent<Speed>( source.CreateEntity(), 7 );

    recs::Registry dest;
    for ( int i = 0; i < 9; i++ )
        dest.AddComponent<Speed>( dest.CreateEntity(), 42 );

    dest = source;

    int count = 0;
    dest.ForEach<Speed>( [&]( recs::Entity, Speed& s ) { count++; assert( s.s == 7 ); } );
    assert( count == 3 );
}

// ================================== runner ==================================

struct TestCase
{
    const char* name;
    void ( *fn )();
};

#define TEST( fn ) TestCase{ #fn, fn }

static const TestCase g_tests[] = {
    // AddComponent / HasComponent
    TEST( test_add_component_then_has_component ),
    TEST( test_has_component_false_for_other_type ),
    TEST( test_has_component_false_for_unregistered_type ),
    TEST( test_has_components_false_for_unregistered_types ),
    TEST( test_add_component_stores_constructor_arguments ),
    TEST( test_register_type_creates_no_entity ),
    // GetComponent
    TEST( test_get_component_returns_live_reference ),
    TEST( test_get_component_is_stable_across_calls ),
    TEST( test_get_component_throws_when_absent ),
    // invalid entity
    TEST( test_has_component_rejects_invalid_entity ),
    TEST( test_get_component_rejects_invalid_entity ),
    TEST( test_add_component_rejects_invalid_entity ),
    TEST( test_create_entity_with_zero_id_rejected ),
    // duplicate add
    TEST( test_duplicate_add_updates_value ),
    TEST( test_duplicate_add_creates_no_second_entry ),
    // GetComponents
    TEST( test_get_components_returns_live_references ),
    TEST( test_get_components_returns_each_component ),
    TEST( test_get_components_throws_when_one_is_absent ),
    // HasComponents
    TEST( test_has_components_single_type ),
    TEST( test_has_components_false_when_one_missing ),
    TEST( test_has_components_true_when_all_present ),
    // entity lookup
    TEST( test_get_entity_by_id_round_trips ),
    TEST( test_get_entity_by_unknown_id_returns_invalid ),
    TEST( test_create_entity_with_id_advances_counter ),
    // ForEach
    TEST( test_foreach_unregistered_type_yields_nothing ),
    TEST( test_foreach_visits_every_entity_with_component ),
    TEST( test_foreach_two_components_yields_intersection ),
    TEST( test_foreach_no_overlap_yields_nothing ),
    TEST( test_foreach_interleaved_ids_yields_only_intersection ),
    TEST( test_foreach_three_components_yields_intersection ),
    TEST( test_foreach_large_population_single_component ),
    TEST( test_foreach_large_population_two_component_join ),
    TEST( test_foreach_functor_not_consumed ),
    // View
    TEST( test_view_unregistered_type_is_empty ),
    TEST( test_view_iterates_in_entity_id_order ),
    TEST( test_view_sees_components_added_later ),
    TEST( test_view_matches_foreach ),
    TEST( test_sparse_join_finds_all_matches ),
    TEST( test_sparse_join_view_matches_foreach ),
    // RuntimeForEach
    TEST( test_runtime_foreach_all_invalid_terminates ),
    TEST( test_runtime_foreach_mixed_ids_intersects ),
    // component removal
    TEST( test_remove_component_clears_has_component ),
    TEST( test_remove_component_clears_from_foreach ),
    TEST( test_remove_component_leaves_other_components ),
    TEST( test_remove_missing_component_is_noop ),
    // entity removal
    TEST( test_remove_entity_leaves_the_others ),
    TEST( test_remove_entity_drops_it_from_registry ),
    TEST( test_remove_entity_does_not_fabricate_entries ),
    TEST( test_has_entity_true_for_live_entity ),
    TEST( test_has_entity_false_after_removal ),
    TEST( test_has_entity_false_for_unknown_entity ),
    TEST( test_has_entity_true_without_components ),
    // batch entity removal
    TEST( test_batch_remove_matches_individual_removal ),
    TEST( test_batch_remove_survivors_stay_reachable ),
    TEST( test_batch_remove_accepts_unsorted_input_with_duplicates ),
    TEST( test_batch_remove_empty_list_is_noop ),
    TEST( test_batch_remove_ignores_unknown_entities ),
    // component id API
    TEST( test_entity_add_component_id_is_idempotent ),
    TEST( test_entity_remove_component_id_removes_once ),
    TEST( test_entity_remove_component_id_twice_is_noop ),
    // entity copying
    TEST( test_copy_entity_across_pool_realloc ),
    TEST( test_copy_entity_is_independent_of_source ),
    // registry copy
    TEST( test_registry_copy_preserves_data ),
    TEST( test_registry_copy_is_isolated_from_original ),
    TEST( test_registry_copy_preserves_entity_count ),
    TEST( test_registry_copy_assign_replaces_destination ),
};

int main( int argc, char** argv )
{
    // Optional substring filter: recs_test <pattern>
    const std::string_view filter = argc > 1 ? argv[1] : std::string_view{};

    size_t run = 0;
    size_t skipped = 0;

    for ( const TestCase& test : g_tests )
    {
        if ( !filter.empty() && std::string_view( test.name ).find( filter ) == std::string_view::npos )
        {
            skipped++;
            continue;
        }

        try
        {
            test.fn();
        }
        catch ( const std::exception& e )
        {
            std::cout << "[FAIL] " << test.name << " threw: " << e.what() << std::endl;
            return 1;
        }

        std::cout << "[ok]   " << test.name << std::endl;
        run++;
    }

    std::cout << std::endl << run << " passed";
    if ( skipped )
        std::cout << ", " << skipped << " filtered out";
    std::cout << std::endl;

    return run == 0 ? 1 : 0;
}
