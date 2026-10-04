#pragma once
#include <concepts>
#include <cstdlib>
#include <optional>

namespace bubble
{
// A reference that may be missing: what a lookup returns. A temporary -
// a result or a parameter, never a member: to keep a link to something
// that can go away, keep its Handle. Becomes std::optional<T&> with C++26.
template <typename T>
class OptRef
{
public:
    OptRef() = default;
    OptRef( std::nullopt_t ) {}
    OptRef( T& value ) : mValue( &value ) {}
    // A mutable reference passes where a const one is asked for.
    template <typename U>
        requires( std::convertible_to<U*, T*> and not std::same_as<U, T> )
    OptRef( OptRef<U> other ) : mValue( other ? &*other : nullptr )
    {
    }

    explicit operator bool() const { return mValue != nullptr; }
    bool HasValue() const { return mValue != nullptr; }

    // Taking what is not there ends the program: it is a bug, not a state.
    T& operator*() const { return Value(); }
    T* operator->() const { return &Value(); }
    T& Value() const
    {
        if ( mValue == nullptr )
            std::abort();
        return *mValue;
    }

    friend bool operator==( OptRef a, OptRef b ) { return a.mValue == b.mValue; }
    friend bool operator==( OptRef a, std::nullopt_t ) { return a.mValue == nullptr; }

private:
    T* mValue = nullptr;
};
}
