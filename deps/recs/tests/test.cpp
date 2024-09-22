#include "recs/registry.hpp"

#include <cassert>
#include <iostream>
#include <string_view>

struct Speed
{
    int s;

    static std::string_view Name()
    {
        return "Speed";
    }
    bool operator==( const Speed& ) const = default;
};

struct Position
{
    int x = 0.0f;
    int y = 0.0f;

    static std::string_view Name()
    {
        return "Position";
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

        int i = 0;
        for ( auto& [speed] : registry.GetView<Speed>() )
            assert( speed.s == i++ );

        for ( auto& entity : entities )
            registry.AddComponent<Speed>( entity, 5.f );

        for ( auto& entity : entities )
            assert( entity.GetComponent<Speed>().s == 5 );

        for ( auto& [speed] : registry.GetView<Speed>() )
            assert( speed.s < 10 );
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
            entity.AddComponent<Speed>( 1.0f )
                  .AddComponent<Position>( i++, 1.0f );
        }

        // foreach
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
            entity.RemoveComponent<Position>();

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
            entity.AddComponent<Speed>( 1.0f )
                .AddComponent<Position>( i++, 1.0f );
        }

        auto view = registry.GetView<Speed, Position>();
        assert( view.Size() == 10 );


        // Clone 
        recs::Registry registry_copy;
        registry.CloneInto( registry_copy );

        auto c_view = registry_copy.GetView<Speed, Position>();
        assert( view.Size() == 10 );

        for ( auto& [s, p] : c_view )
            s.s = 5;

        for ( auto& [s, p] : view )
            assert( s.s != 5 );
    }

    return 0;
}