#include <iostream>
#include <cassert>
#include <print>
#include <ranges>
#include "fixed_size_function.hpp"
#include "fixed_size_packaged_task.hpp"
#include "thread_pool.hpp"


void* operator new ( size_t size, const char* filename, int line )
{
    assert( false );
    void* ptr = new char[size];
    return ptr;
}

int mulFunc( int x, int y )
{
    return x * y;
}

void printFunc()
{
    std::print( "hello" );
}


int main( void )
{
    // Fixed size function
    {
        int i = 0;
        FixedSizeFunction<int( int, int )> mul( [&]( int a, int b ){ return a * b + i; } );
        FixedSizeFunction<int( int, int )> sum( [&]( int a, int b ){ return a + b + i; } );

        assert( mul( 2, 3 ) == 6 );
        assert( sum( 2, 3 ) == 5 );
        std::swap( mul, sum );
        assert( mul( 2, 3 ) == 5 );
        assert( sum( 2, 3 ) == 6 );
    }
    {
        FixedSizeFunction<int( int, int )> mul( []( int a, int b ){ return a * b; } );
        assert( mul( 2, 3 ) == 6 );
    }
    {
        FixedSizeFunction<int( int, int )> mul( mulFunc );
        assert( mul( 2, 3 ) == 6 );
    }


    // Fixed size packaged task
    {
        std::future<int> future;
        {
            FixedSizePackagedTask<int( int, int )> mulTask( []( int a, int b ){ return a * b; } );
            future = mulTask.get_future();
            mulTask( 2, 3 );
        }
        assert( future.get() == 6 );
    }
    {
        std::future<int> future;
        {
            FixedSizePackagedTask<int( int, int )> mulTask( mulFunc );
            future = mulTask.get_future();
            mulTask( 2, 3 );
        }
        assert( future.get() == 6 );
    }


    // Thread pool
    {
        ThreadPool threadPool;
        for ( size_t i = 0; i < 10; i++ )
        {
            threadPool.AddTask( [i](){ std::println( "hello i:{} thread:{}", i, std::this_thread::get_id() ); } );
        }
        std::println( "End will be in destructor" );
    }

    std::println( "\nFixedSizeFunction size {}", sizeof( FixedSizeFunction<void( void )> ) );
    std::println( "FixedSizePackagedTask size {}\n", sizeof( FixedSizePackagedTask<void( void )> ) );
    std::println( "std::packaged_task (Allocate heap memory) size {}\n", sizeof( std::packaged_task<void( void )> ) );

    {
        ThreadPool threadPool;

        // create tasks
        std::vector<FixedSizePackagedTask<void( void )>> tasks;
        for ( size_t i = 0; i < 10; i++ )
            tasks.emplace_back( [](){ std::println( "hello future thread:{}", std::this_thread::get_id() ); } );

        // add tasks to thread pool
        threadPool.AddTasks( tasks );

        // wait for all tasks
        WaitForAll( tasks );

        std::print( "\nfuture end\n" );
    }
    return 0;
}
