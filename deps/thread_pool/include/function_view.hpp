#pragma once
#include <type_traits>

template <typename Signature>
class FunctionView;

template <typename Ret, typename... Args>
class FunctionView<Ret( Args... )>
{
    using FunctionPtrType = Ret( * )( Args... );
    using CallableType = Ret( * )( void* toCall, Args&&... args );

public:
    FunctionView() = default;

    template <typename FuncObj>
    FunctionView( FuncObj& functionalObject )
    {
        mPtr = &functionalObject;
        mCallable = []( void* toCall, Args&&... args )
        {
            reinterpret_cast<FuncObj*>( FuncObj )->operator()( std::forward<Args>( args )... );
        }
    }

    template <typename FuncRet, typename... FuncArgs>
    FixedSizeFunction( FuncRet( *func )( FuncArgs... ) )
    {
        mPtr = func;
        mCallable = []( void* toCall, Args&&... args ) -> Ret
        {
            return reinterpret_cast<FuncRet( * )( FuncArgs... )>( toCall )( std::forward<Args>( args )... );
        };
    }

    FunctionView( const FunctionView& other )
    {
        mPtr = other.mPtr;
        mCallable = other.mCallable;
    }

    FunctionView& operator=( const FunctionView& other )
    {
        mPtr = other.mPtr;
        mCallable = other.mCallable;
    }

    Ret operator()( Args&&... args )
    {
        if ( not mCallable )
            throw std::runtime_error( "Callable not set" );
        return mCallable( mStorage, mFunctionPtr, std::forward<Args>( args )... );
    }

private:
    CallableType mCallable = nullptr;
    void* mPtr;

};