#include "recs/registry.hpp"

#include <cassert>
#include <iostream>
#include <string_view>
#include <stdexcept>
#include <memory>
#include <vector>
#include <array>

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

// ---- basic: add, get, has, modify ----
void test_basic_component_ops()
{
    recs::Registry registry;
    recs::Entity entity = registry.CreateEntity();

    registry.AddComponent<Speed>( entity, 5 );
    assert( registry.HasComponent<Speed>( entity ) );
    assert( !registry.HasComponent<Position>( entity ) );

    Speed& speed = registry.GetComponent<Speed>( entity );
    speed.s += 1;
    assert( speed == registry.GetComponent<Speed>( entity ) );
    assert( registry.GetComponent<Speed>( entity ).s == 6 );
}

// ---- INVALID_ENTITY throws on mutating operations ----
void test_invalid_entity_throws()
{
    recs::Registry registry;
    registry.AddComponent<Speed>();

    bool caught = false;
    try { registry.HasComponent<Speed>( recs::INVALID_ENTITY ); }
    catch ( std::exception& ) { caught = true; }
    assert( caught );

    caught = false;
    try { registry.GetComponent<Speed>( recs::INVALID_ENTITY ); }
    catch ( std::exception& ) { caught = true; }
    assert( caught );

    caught = false;
    try { registry.AddComponent<Speed>( recs::INVALID_ENTITY ); }
    catch ( std::exception& ) { caught = true; }
    assert( caught );
}

// ---- HasComponent returns false for unregistered type ----
void test_has_component_unregistered()
{
    recs::Registry registry;
    recs::Entity entity = registry.CreateEntity();
    assert( !registry.HasComponent<Speed>( entity ) );
    assert( (!registry.HasComponents<Speed, Position>( entity )) );
}

// ---- duplicate AddComponent updates value, not duplicates ----
void test_duplicate_add_updates()
{
    recs::Registry registry;
    recs::Entity entity = registry.CreateEntity();

    registry.AddComponent<Speed>( entity, 10 );
    assert( registry.GetComponent<Speed>( entity ).s == 10 );

    registry.AddComponent<Speed>( entity, 99 );
    assert( registry.GetComponent<Speed>( entity ).s == 99 );

    int count = 0;
    registry.ForEach<Speed>( [&]( recs::Entity, Speed& ) { count++; } );
    assert( count == 1 );
}

// ---- GetComponents (multi-component tuple) ----
void test_get_components_tuple()
{
    recs::Registry registry;
    recs::Entity entity = registry.CreateEntity();

    registry.AddComponent<Speed>( entity, 1 );
    registry.AddComponent<Position>( entity, 3, 7 );

    auto [speed, position] = registry.GetComponents<Speed, Position>( entity );
    speed.s += 1;
    assert( speed == registry.GetComponent<Speed>( entity ) );
    assert( position.x == 3 );
    assert( position.y == 7 );
}

// ---- GetEntityById ----
void test_get_entity_by_id()
{
    recs::Registry registry;
    recs::Entity e1 = registry.CreateEntity();
    recs::Entity e2 = registry.CreateEntity();

    assert( registry.GetEntityById( (size_t)e1 ) == e1 );
    assert( registry.GetEntityById( (size_t)e2 ) == e2 );
    assert( registry.GetEntityById( 9999 ) == recs::INVALID_ENTITY );
}

// ---- view-based iteration order ----
void test_view_iteration()
{
    recs::Registry registry;

    std::vector<recs::Entity> empty_entities;
    for ( int i = 0; i < 10; i++ )
        empty_entities.push_back( registry.CreateEntity() );

    for ( int i = 0; i < 10; i++ )
    {
        recs::Entity entity = registry.CreateEntity();
        registry.AddComponent<Speed>( entity, i );
    }

    int i = 0;
    for ( auto&& [speed] : registry.GetView<Speed>() )
        assert( speed.s == i++ );

    for ( auto entity : empty_entities )
        registry.AddComponent<Speed>( entity, 5 );

    for ( auto entity : empty_entities )
        assert( registry.GetComponent<Speed>( entity ).s == 5 );

    for ( auto&& [speed] : registry.GetView<Speed>() )
        assert( speed.s < 10 );
}

// ---- comprehensive lifecycle: ForEach, RemoveComponent, RemoveEntity ----
void test_comprehensive_lifecycle()
{
    recs::Registry registry;

    std::vector<recs::Entity> entities;
    for ( int i = 0; i < 10; i++ )
        entities.push_back( registry.CreateEntity() );

    for ( int i = 0; i < 10; i++ )
    {
        recs::Entity entity = registry.CreateEntity();
        registry.AddComponent<Speed>( entity, i );
    }

    int i = 0;
    for ( auto entity : entities )
    {
        registry.AddComponent<Speed>( entity, 1 );
        registry.AddComponent<Position>( entity, i++, 1 );
    }

    int count = 0;
    registry.ForEach<Speed, Position>( [&]( recs::Entity, Speed&, Position& ) { count++; } );
    assert( count == 10 );

    count = 0;
    registry.ForEach<Speed>( [&]( recs::Entity, Speed& ) { count++; } );
    assert( count == 20 );

    for ( auto entity : entities )
        registry.RemoveComponent<Position>( entity );

    count = 0;
    registry.ForEach<Position>( [&]( recs::Entity, Position& ) { count++; } );
    assert( count == 0 );

    for ( auto entity : entities )
        assert( !registry.HasComponent<Position>( entity ) );

    for ( auto entity : entities )
        registry.RemoveEntity( entity );

    count = 0;
    registry.ForEach<Speed>( [&]( recs::Entity, Speed& ) { count++; } );
    assert( count == 10 );
}

// ---- ForEach on unregistered/empty pool yields 0 results, no throw ----
void test_foreach_empty_pool()
{
    recs::Registry registry;
    int count = 0;
    registry.ForEach<Speed>( [&]( recs::Entity, Speed& ) { count++; } );
    assert( count == 0 );

    auto view = registry.GetView<Speed>();
    assert( view.Size() == 0 );
}

// ---- ForEach<A, B> where entities have A or B but NOT both -> 0 results ----
void test_foreach_no_overlap()
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

// ---- ForEach<A, B> with interleaved entity IDs: only intersection emitted ----
void test_foreach_interleaved_ids()
{
    recs::Registry registry;

    recs::Entity e1 = registry.CreateEntity();
    recs::Entity e2 = registry.CreateEntity();
    recs::Entity e3 = registry.CreateEntity();

    // Speed pool: [e1, e3], Position pool: [e2, e3] — only e3 has both
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

// ---- RemoveComponent on entity without it is a no-op ----
void test_remove_missing_component_noop()
{
    recs::Registry registry;
    recs::Entity entity = registry.CreateEntity();
    registry.AddComponent<Speed>( entity, 1 );

    registry.RemoveComponent<Position>( entity );
    assert( registry.HasComponent<Speed>( entity ) );
    assert( !registry.HasComponent<Position>( entity ) );
}

// ---- three-component query ----
void test_three_component_query()
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

// ---- large entity population ----
void test_large_population()
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

    int speed_count = 0;
    registry.ForEach<Speed>( [&]( recs::Entity, Speed& ) { speed_count++; } );
    assert( speed_count == N );

    int both_count = 0;
    registry.ForEach<Speed, Position>( [&]( recs::Entity, Speed& s, Position& p )
    {
        assert( s.s == p.x );
        both_count++;
    } );
    assert( both_count == N / 2 );
}

// ---- registry copy: correct data, isolated from original ----
void test_registry_copy()
{
    recs::Registry registry;

    std::vector<recs::Entity> entities;
    for ( int i = 0; i < 10; i++ )
        entities.push_back( registry.CreateEntity() );

    for ( int i = 0; i < 10; i++ )
    {
        recs::Entity entity = registry.CreateEntity();
        registry.AddComponent<Speed>( entity, i );
    }

    int i = 0;
    for ( auto entity : entities )
    {
        registry.AddComponent<Speed>( entity, 1 );
        registry.AddComponent<Position>( entity, i++, 1 );
    }

    // Collect original data
    std::vector<std::tuple<Speed, Position>> original;
    registry.ForEach<Speed, Position>( [&]( recs::Entity, Speed& s, Position& p )
    {
        original.emplace_back( s, p );
    } );
    assert( original.size() == 10 );

    // Copy registry and verify data matches
    recs::Registry copy = registry;
    std::vector<std::tuple<Speed, Position>> copied;
    copy.ForEach<Speed, Position>( [&]( recs::Entity, Speed& s, Position& p )
    {
        copied.emplace_back( s, p );
    } );
    assert( original == copied );

    // Mutate copy, original unchanged
    copy.ForEach<Speed>( [&]( recs::Entity, Speed& s ) { s.s = 999; } );
    registry.ForEach<Speed>( [&]( recs::Entity, Speed& s ) { assert( s.s != 999 ); } );

    // Copy has full entity count
    int copy_count = 0;
    copy.ForEach<Speed>( [&]( recs::Entity, Speed& ) { copy_count++; } );
    assert( copy_count == 20 );
}

// ---- View matches ForEach results ----
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
    {
        recs::Entity e = registry.CreateEntity();
        registry.AddComponent<Speed>( e, i * 100 ); // speed only, no position
    }

    std::vector<std::pair<int,int>> from_foreach;
    registry.ForEach<Speed, Position>( [&]( recs::Entity, Speed& s, Position& p )
    {
        from_foreach.emplace_back( s.s, p.x );
    } );

    std::vector<std::pair<int,int>> from_view;
    for ( auto&& [s, p] : registry.GetView<Speed, Position>() )
        from_view.emplace_back( s.s, p.x );

    assert( from_foreach == from_view );
}

// ---- HasComponents (multi) ----
void test_has_components_multi()
{
    recs::Registry registry;
    recs::Entity e = registry.CreateEntity();
    registry.AddComponent<Speed>( e, 1 );

    assert( registry.HasComponents<Speed>( e ) );
    assert( (!registry.HasComponents<Speed, Position>( e )) );

    registry.AddComponent<Position>( e, 0, 0 );
    assert( (registry.HasComponents<Speed, Position>( e )) );
}

// ---- regression: CopyEntity across a pool reallocation ----
// The source component address must be re-read after PushEmpty, which can
// reallocate and shift the pool out from under a cached pointer.
void test_copy_entity_across_realloc()
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

    // Copying repeatedly forces the pool through several growth steps.
    for ( size_t i = 0; i < entities.size(); i++ )
    {
        recs::Entity source = entities[i];
        const Position expected = registry.GetComponent<Position>( source );
        recs::Entity copy = registry.CopyEntity( source );

        assert( registry.GetComponent<Position>( copy ) == expected );
        assert( registry.GetComponent<Position>( source ) == expected );
    }
}

// ---- regression: copy-assigning a registry destroys the destination's data ----
void test_registry_copy_assign_replaces()
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

// ---- regression: a pool holds at most one slot per entity ----
void test_no_duplicate_component_slots()
{
    recs::Registry registry;
    registry.AddComponent<Speed>();

    recs::Entity e = registry.CreateEntity();
    registry.EntityAddComponentId( e, Speed::ID() );
    registry.EntityAddComponentId( e, Speed::ID() );
    registry.EntityAddComponentId( e, Speed::ID() );

    int count = 0;
    registry.ForEach<Speed>( [&]( recs::Entity, Speed& ) { count++; } );
    assert( count == 1 );

    // Removing once must fully remove it.
    registry.EntityRemoveComponentId( e, Speed::ID() );
    assert( !registry.HasComponent<Speed>( e ) );

    count = 0;
    registry.ForEach<Speed>( [&]( recs::Entity, Speed& ) { count++; } );
    assert( count == 0 );

    // Removing again is a no-op rather than an underflow.
    registry.EntityRemoveComponentId( e, Speed::ID() );
    assert( !registry.HasComponent<Speed>( e ) );
}

// ---- regression: RuntimeForEach with no valid component id terminates ----
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

// ---- regression: RuntimeForEach still intersects when some ids are invalid ----
void test_runtime_foreach_mixed_ids()
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

// ---- regression: the functor is not moved-from after the first element ----
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

// ---- regression: id 0 collides with INVALID_ENTITY ----
void test_create_entity_with_zero_id_rejected()
{
    recs::Registry registry;
    bool caught = false;
    try { registry.CreateEntityWithId( 0 ); }
    catch ( std::exception& ) { caught = true; }
    assert( caught );
    assert( registry.Size() == 0 );
}

// ---- regression: RemoveEntity on an unknown entity does not fabricate one ----
void test_remove_unknown_entity_does_not_insert()
{
    recs::Registry registry;
    registry.CreateEntity();
    assert( registry.Size() == 1 );

    registry.RemoveEntity( registry.CreateEntity() );
    assert( registry.Size() == 1 );
}

int main( void )
{
    test_basic_component_ops();
    test_invalid_entity_throws();
    test_has_component_unregistered();
    test_duplicate_add_updates();
    test_get_components_tuple();
    test_get_entity_by_id();
    test_view_iteration();
    test_comprehensive_lifecycle();
    test_foreach_empty_pool();
    test_foreach_no_overlap();
    test_foreach_interleaved_ids();
    test_remove_missing_component_noop();
    test_three_component_query();
    test_large_population();
    test_registry_copy();
    test_view_matches_foreach();
    test_has_components_multi();
    test_copy_entity_across_realloc();
    test_registry_copy_assign_replaces();
    test_no_duplicate_component_slots();
    test_runtime_foreach_all_invalid_terminates();
    test_runtime_foreach_mixed_ids();
    test_foreach_functor_not_consumed();
    test_create_entity_with_zero_id_rejected();
    test_remove_unknown_entity_does_not_insert();

    std::cout << "All tests passed!\n";
    return 0;
}
