// recs microbenchmarks.
//
//   recs_benchmark [entity_count] [repeats]
//
// Each case is run `repeats` times and the *best* run is reported, which filters
// out scheduler noise without hiding real regressions. Build in a release
// configuration - debug numbers are dominated by MSVC iterator debugging and
// tell you nothing about the library.

#include "recs/registry.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <random>
#include <string>
#include <vector>

using Clock = std::chrono::steady_clock;

// ============================== components ==================================

struct Transform { float m[16] = {}; static int ID() { return 0; } };
struct Velocity  { float x = 0, y = 0, z = 0; static int ID() { return 1; } };
struct Health    { int hp = 100; static int ID() { return 2; } };
struct Rare      { int v = 0; static int ID() { return 3; } };

// Keeps the optimiser from deleting the work being measured.
volatile size_t g_sink = 0;

// =============================== harness ====================================

static int    g_entityCount = 25000;
static int    g_repeats     = 3;
static size_t g_lookups     = 200000;

static double Millis( Clock::time_point a, Clock::time_point b )
{
    return std::chrono::duration<double, std::milli>( b - a ).count();
}

static void Report( const char* name, double ms, size_t ops )
{
    std::printf( "  %-50s %9.2f ms %12.1f ns/op\n", name, ms, ops ? ms * 1e6 / ops : 0.0 );
}

static void Section( const char* title )
{
    std::printf( "\n%s\n", title );
    std::printf( "  %s\n", std::string( 74, '-' ).c_str() );
}

// Runs `body` `repeats` times, calling `reset` before each, and reports the
// fastest. `reset` is not timed, so destructive cases can rebuild their state.
template <typename Reset, typename Body>
static void Bench( const char* name, size_t ops, int repeats, Reset&& reset, Body&& body )
{
    double best = std::numeric_limits<double>::max();
    for ( int r = 0; r < repeats; r++ )
    {
        reset();
        const auto start = Clock::now();
        body();
        best = std::min( best, Millis( start, Clock::now() ) );
    }
    Report( name, best, ops );
}

// Non-destructive case: no per-run setup needed.
template <typename Body>
static void Bench( const char* name, size_t ops, Body&& body )
{
    Bench( name, ops, g_repeats, [] {}, body );
}

// ============================== fixtures ====================================

// N entities, all with Transform, every second one with Velocity, all with Health.
static void FillRegistry( recs::Registry& registry, std::vector<recs::Entity>& entities )
{
    entities.clear();
    entities.reserve( g_entityCount );

    for ( int i = 0; i < g_entityCount; i++ )
        entities.push_back( registry.CreateEntity() );

    for ( int i = 0; i < g_entityCount; i++ )
        registry.AddComponent<Transform>( entities[i] );
    for ( int i = 0; i < g_entityCount; i += 2 )
        registry.AddComponent<Velocity>( entities[i] );
    for ( int i = 0; i < g_entityCount; i++ )
        registry.AddComponent<Health>( entities[i] );
}

// ============================ benchmark cases ===============================

static void BenchConstruction( std::mt19937& rng )
{
    Section( "Construction" );

    {
        recs::Registry registry;
        Bench( "CreateEntity", g_entityCount, g_repeats,
            [&] { registry = recs::Registry{}; },
            [&]
            {
                for ( int i = 0; i < g_entityCount; i++ )
                    registry.CreateEntity();
            } );
    }

    // Ascending ids hit the pool's append path.
    {
        recs::Registry registry;
        std::vector<recs::Entity> entities;
        Bench( "AddComponent, ascending entity id", g_entityCount, g_repeats,
            [&]
            {
                registry = recs::Registry{};
                entities.clear();
                for ( int i = 0; i < g_entityCount; i++ )
                    entities.push_back( registry.CreateEntity() );
            },
            [&]
            {
                for ( recs::Entity e : entities )
                    registry.AddComponent<Transform>( e );
            } );
    }

    // Random ids force an ordered insert, which shifts the tail of the pool.
    // This is the quadratic path - one repeat is plenty.
    {
        recs::Registry registry;
        std::vector<recs::Entity> shuffled;
        Bench( "AddComponent, random entity order", g_entityCount, 1,
            [&]
            {
                registry = recs::Registry{};
                shuffled.clear();
                for ( int i = 0; i < g_entityCount; i++ )
                    shuffled.push_back( registry.CreateEntity() );
                std::shuffle( shuffled.begin(), shuffled.end(), rng );
            },
            [&]
            {
                for ( recs::Entity e : shuffled )
                    registry.AddComponent<Transform>( e );
            } );
    }
}

static void BenchRandomAccess( std::mt19937& rng )
{
    Section( "Random access" );

    recs::Registry registry;
    std::vector<recs::Entity> entities;
    FillRegistry( registry, entities );

    std::vector<recs::Entity> lookups;
    lookups.reserve( g_lookups );
    for ( size_t i = 0; i < g_lookups; i++ )
        lookups.push_back( entities[rng() % entities.size()] );

    Bench( "GetComponent", lookups.size(), [&]
    {
        size_t acc = 0;
        for ( recs::Entity e : lookups )
            acc += (size_t)registry.GetComponent<Transform>( e ).m[0];
        g_sink = acc;
    } );

    Bench( "HasComponent", lookups.size(), [&]
    {
        size_t acc = 0;
        for ( recs::Entity e : lookups )
            acc += registry.HasComponent<Transform>( e );
        g_sink = acc;
    } );

    Bench( "GetComponents<Transform, Health>", lookups.size(), [&]
    {
        size_t acc = 0;
        for ( recs::Entity e : lookups )
        {
            auto [t, h] = registry.GetComponents<Transform, Health>( e );
            acc += h.hp;
        }
        g_sink = acc;
    } );
}

static void BenchIteration( std::mt19937& rng )
{
    Section( "Iteration" );

    recs::Registry registry;
    std::vector<recs::Entity> entities;
    FillRegistry( registry, entities );

    Bench( "ForEach<Transform>", g_entityCount, [&]
    {
        size_t acc = 0;
        registry.ForEach<Transform>( [&]( recs::Entity, Transform& t ) { acc += (size_t)t.m[0]; } );
        g_sink = acc;
    } );

    Bench( "ForEach<Transform, Health>", g_entityCount, [&]
    {
        size_t acc = 0;
        registry.ForEach<Transform, Health>( [&]( recs::Entity, Transform&, Health& h ) { acc += h.hp; } );
        g_sink = acc;
    } );

    Bench( "View<Transform, Health>", g_entityCount, [&]
    {
        size_t acc = 0;
        for ( auto&& [t, h] : registry.GetView<Transform, Health>() )
            acc += h.hp;
        g_sink = acc;
    } );

    Bench( "ForEach<Transform, Velocity>, half match", g_entityCount / 2, [&]
    {
        size_t acc = 0;
        registry.ForEach<Transform, Velocity>( [&]( recs::Entity, Transform&, Velocity& v ) { acc += (size_t)v.x; } );
        g_sink = acc;
    } );

    // Sparse intersection. The join has to skip most of the Transform pool, so
    // this is what the galloping advance in Pool::AdvanceTo exists for: the
    // per-match cost should stay flat as the entity count grows.
    constexpr int kRareCount = 10;
    for ( int i = 0; i < kRareCount; i++ )
        registry.AddComponent<Rare>( entities[rng() % entities.size()] );

    Bench( "ForEach<Rare, Transform>, 10 of N match", kRareCount, [&]
    {
        size_t acc = 0;
        registry.ForEach<Rare, Transform>( [&]( recs::Entity, Rare& r, Transform& ) { acc += r.v; } );
        g_sink = acc;
    } );
}

static void BenchMutation( std::mt19937& rng )
{
    Section( "Mutation" );

    {
        recs::Registry registry;
        std::vector<recs::Entity> shuffled;
        auto reset = [&]
        {
            registry = recs::Registry{};
            std::vector<recs::Entity> entities;
            FillRegistry( registry, entities );
            shuffled = entities;
            std::shuffle( shuffled.begin(), shuffled.end(), rng );
        };

        // Quadratic: each removal shifts the tail of every pool the entity is in.
        Bench( "RemoveEntity, one call per entity", g_entityCount, 1, reset, [&]
        {
            for ( recs::Entity e : shuffled )
                registry.RemoveEntity( e );
        } );

        Bench( "RemoveEntities, single batch call", g_entityCount, g_repeats, reset, [&]
        {
            registry.RemoveEntities( shuffled );
        } );
    }

    {
        recs::Registry registry;
        std::vector<recs::Entity> entities;
        FillRegistry( registry, entities );

        Bench( "Registry deep copy", 1, [&]
        {
            recs::Registry copy = registry;
            g_sink = copy.Size();
        } );
    }
}

// Scene loading feeds pools from a JSON object whose keys are entity ids as
// *strings*, so they arrive as "1", "10", "100", "1000", ..., "11", "110".
// Every backwards jump shifts the pool tail. Sorting numerically first turns
// each insert back into an append.
static void BenchLoadOrder()
{
    Section( "Bulk load order (scene deserialisation)" );

    std::vector<size_t> lexicographic;
    for ( int i = 1; i <= g_entityCount; i++ )
        lexicographic.push_back( i );
    std::sort( lexicographic.begin(), lexicographic.end(), []( size_t a, size_t b )
    {
        return std::to_string( a ) < std::to_string( b );
    } );

    std::vector<size_t> numeric;
    for ( int i = 1; i <= g_entityCount; i++ )
        numeric.push_back( i );

    auto benchOrder = [&]( const char* name, const std::vector<size_t>& order, int repeats )
    {
        recs::Registry registry;
        Bench( name, order.size(), repeats,
            [&]
            {
                registry = recs::Registry{};
                registry.AddComponent<Transform>();
                for ( size_t id : order )
                    registry.CreateEntityWithId( id );
            },
            [&]
            {
                for ( size_t id : order )
                    registry.EntityAddComponentId( registry.GetEntityById( id ), Transform::ID() );
            } );
    };

    benchOrder( "fill in JSON key order (unsorted)", lexicographic, 1 );
    benchOrder( "fill in numeric id order (sorted first)", numeric, g_repeats );
}

// ================================= main =====================================

int main( int argc, char** argv )
{
    if ( argc > 1 ) g_entityCount = std::atoi( argv[1] );
    if ( argc > 2 ) g_repeats     = std::atoi( argv[2] );

    if ( g_entityCount <= 0 || g_repeats <= 0 )
    {
        std::fprintf( stderr, "usage: %s [entity_count] [repeats]\n", argv[0] );
        return 1;
    }

    std::printf( "recs benchmark\n" );
    std::printf( "  entities: %d,  repeats: %d (best run reported),  lookups: %zu\n",
                 g_entityCount, g_repeats, g_lookups );

    std::mt19937 rng( 12345 );

    BenchConstruction( rng );
    BenchRandomAccess( rng );
    BenchIteration( rng );
    BenchMutation( rng );
    BenchLoadOrder();

    std::printf( "\n" );
    return 0;
}
