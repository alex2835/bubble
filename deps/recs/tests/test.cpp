#include "recs/registry.hpp"

#include <cassert>
#include <iostream>
#include <string_view>

enum class Component
{
    Speed,
    Position
};

struct Speed
{
    int s;

    static size_t ID()
    {
        return static_cast<size_t>( Component::Speed );
    }
    bool operator==( const Speed& ) const = default;
};

struct Position
{
    int x = 0.0f;
    int y = 0.0f;

    static size_t ID()
    {
        return static_cast<size_t>( Component::Position );
    }
    bool operator==( const Position& ) const = default;
};

int main( void )
{
    //{
    //    // create entity 
    //    recs::Registry registry;

    //    recs::Entity entity = registry.CreateEntity();

    //    registry.AddComponent<Speed>( entity, 0.0f );
    //    assert( registry.HasComponent<Speed>( entity ) );
    //    assert( !registry.HasComponent<Position>( entity ) );

    //    Speed& speed_component = registry.GetComponent<Speed>( entity );
    //    speed_component.s += 1.0f;
    //    assert( speed_component == registry.GetComponent<Speed>( entity ) );

    //    try
    //    {
    //        registry.HasComponent<Speed>( recs::INVALID_ENTITY );
    //    }
    //    catch ( std::exception& e )
    //    {
    //        assert( std::string( e.what() ) == "Has component: invalid entity" );
    //    }
    //}

    {
        recs::Registry registry;
        // get component
        recs::Entity entity = registry.CreateEntity();

        registry.AddComponent<Speed>( entity, 1.0f );
        registry.AddComponent<Position>( entity, 0.0f, 0.0f );

        auto [speed, position] = registry.GetComponents<Speed, Position>( entity );

        speed.s += 1.0f;
        assert( speed == registry.GetComponent<Speed>( entity ) );
    }

    {
        recs::Registry registry;

        std::vector<recs::Entity> entities;
        for ( int i = 0; i < 10; i++ )
            entities.push_back( registry.CreateEntity() );

        for ( int i = 0; i < 10; i++ )
        {
            recs::Entity entity = registry.CreateEntity();
            registry.AddComponent<Speed>( entity, (float)i );
        }

        //int i = 0;
        //for ( auto& [speed] : registry.GetView<Speed>() )
        //    assert( speed.s == i++ );
        //
        //for ( auto& entity : entities )
        //    registry.AddComponent<Speed>( entity, 5.f );
        //
        //for ( auto& entity : entities )
        //    assert( entity.GetComponent<Speed>().s == 5 );
        //
        //for ( auto& [speed] : registry.GetView<Speed>() )
        //    assert( speed.s < 10 );
    }

    {
        recs::Registry registry;

        std::vector<recs::Entity> entities;
        for ( int i = 0; i < 10; i++ )
        {
            entities.push_back( registry.CreateEntity() );
        }
        for ( int i = 0; i < 10; i++ )
        {
            recs::Entity entity = registry.CreateEntity();
            registry.AddComponent<Speed>( entity, i );
        }

        int i = 0;
        for ( auto entity : entities )
        {
            registry.AddComponent<Speed>( entity, 1.0f )
                    .AddComponent<Position>( entity, i++, 1.0f );
        }

        // ForEach
        int count = 0;
        registry.ForEach<Speed, Position>( [&]( recs::Entity entity, Speed& speed, Position& position )
        {
            count++;
        } );
        assert( count == 10 );

        count = 0;
        registry.ForEach<Speed>( [&]( recs::Entity entity, Speed& speed )
        {
            count++;
        } );
        assert( count == 20 );

        // remove component
        for ( auto entity : entities )
            registry.RemoveComponent<Position>( entity );

        count = 0;
        registry.ForEach<Position>( [&]( recs::Entity entity, Position& speed )
        {
            count++;
        } );
        assert( count == 0 );

        // remove entity
        for ( auto entity : entities )
            registry.RemoveEntity( entity );

        count = 0;
        registry.ForEach<Speed>( [&]( recs::Entity entity, Speed& speed )
        {
            count++;
        } );
        assert( count == 10 );
    }

    {
        recs::Registry registry;

        std::vector<recs::Entity> entities;
        for ( int i = 0; i < 10; i++ )
        {
            entities.push_back( registry.CreateEntity() );
        }
        for ( int i = 0; i < 10; i++ )
        {
            recs::Entity entity = registry.CreateEntity();
            registry.AddComponent<Speed>( entity, i );
        }

        int i = 0;
        for ( auto entity : entities )
        {
            registry.AddComponent<Speed>( entity, 1.0f )
                    .AddComponent<Position>( entity, Position{ .x=i++, .y=1 } );
        }


        // Test copy registry
        std::vector<std::tuple<Speed, Position>> components;
        registry.ForEach<Speed, Position>( [&components]( recs::Entity _, Speed& speed, Position& position )
        {
            components.emplace_back( std::make_tuple( speed, position ) );
        } );

        recs::Registry registryCopy = registry;
        std::vector<std::tuple<Speed, Position>> copyComponents;
        registryCopy.ForEach<Speed, Position>( [&copyComponents]( recs::Entity _, Speed& speed, Position& position )
        {
            copyComponents.emplace_back( std::make_tuple( speed, position ) );
        } );
        assert( components == copyComponents );
        
    }

    return 0;
}