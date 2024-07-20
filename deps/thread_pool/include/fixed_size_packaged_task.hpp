#pragma once
#include <array>
#include <future>
#include <type_traits>
#include "fixed_size_function.hpp"

template <typename Signature, size_t StorageSize = 64>
class FixedSizePackagedTask;

template <typename Ret, typename... Args, size_t StorageSize>
class FixedSizePackagedTask<Ret( Args... ), StorageSize>
{
    using CallableType = void( * )( FixedSizeFunction<Ret( Args... ), StorageSize>& func,
                                    std::promise<Ret>& promise,
                                    Args&&... args );

public:
    FixedSizePackagedTask() = delete;
    FixedSizePackagedTask( FixedSizePackagedTask& ) = delete;
    FixedSizePackagedTask& operator=( FixedSizePackagedTask& ) = delete;

    FixedSizePackagedTask( FixedSizePackagedTask&& ) = default;
    FixedSizePackagedTask& operator=( FixedSizePackagedTask&& ) = default;

    template <typename FuncObj>
    FixedSizePackagedTask( FuncObj&& func )
        : mFunction( std::forward<FuncObj>( func ) )
    {
        mCallable = []( FixedSizeFunction<Ret( Args... ), StorageSize>& func,
                        std::promise<Ret>& promise,
                        Args&&... args ) -> void
        {
            try
            {
                if constexpr ( std::is_void_v<std::invoke_result_t<FuncObj, Args...>> )
                {
                    func( std::forward<Args>( args )... );
                    promise.set_value();
                }
                else
                    promise.set_value( func( std::forward<Args>( args )... ) );
            }
            catch ( std::exception_ptr e )
            {
                promise.set_exception( e );
            }
        };
    }

    std::future<Ret> get_future()
    {
        return mPromise.get_future();
    }

    void wait()
    {
        return mPromise.get_future().wait();
    }

    auto get()
    {
        return std::move( mPromise.get_future().get() );
    }

    void operator()( Args&&... args )
    {
        mCallable( mFunction, mPromise, std::forward<Args>( args )... );
    }

private:
    FixedSizeFunction<Ret( Args... ), StorageSize> mFunction;
    std::promise<Ret> mPromise;
    CallableType mCallable;
};

