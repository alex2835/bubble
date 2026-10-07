#pragma once
#include "bubble/types/containers.hpp"
#include "bubble/types/number.hpp"
#include "bubble/types/opt_ref.hpp"
#include "bubble/types/utility.hpp"
#include <deque>
#include <functional>

namespace bubble
{
// A link to something that can go away: a slot's index and the slot's
// generation when it was handed out. Removing the entry bumps the
// generation, so every handle to it stops working at once - a stale handle
// finds nothing instead of whatever moved in after. Plain numbers, copied and
// compared freely, but good for this run only: what is saved is an `...Id`.
// `Tag` keeps handles of different things apart.
template <typename Tag>
struct Handle
{
    static constexpr u32 cNone = ~0u;

    u32 mIndex = cNone;
    u32 mGeneration = 0;

    explicit operator bool() const { return mIndex != cNone; }
    friend bool operator==( Handle, Handle ) = default;
};

// Owns entries and hands out handles to them. Add, Remove, Get and Alive
// are O(1). An entry stays where it is until it is removed - adding others
// does not move it - so a reference from Get lasts until its own Remove.
// Freed slots are reused, latest first, so the layout and the order of Handles
// follow from the order of calls alone.
template <typename T, typename Tag>
class SlotMap
{
public:
    Handle<Tag> Add( T value )
    {
        u32 index = 0;
        if ( mFree.empty() )
        {
            index = static_cast<u32>( mSlots.size() );
            mSlots.emplace_back();
        }
        else
        {
            index = mFree.back();
            mFree.pop_back();
        }
        Slot& slot = mSlots[index];
        slot.mValue.emplace( std::move( value ) );
        ++mSize;
        return Handle<Tag>{ index, slot.mGeneration };
    }

    // False when the handle was stale already.
    bool Remove( Handle<Tag> handle )
    {
        if ( not Alive( handle ) )
            return false;
        Slot& slot = mSlots[handle.mIndex];
        // The generation moves first: code the destructor runs sees the
        // entry as gone.
        ++slot.mGeneration;
        slot.mValue.reset();
        mFree.push_back( handle.mIndex );
        --mSize;
        return true;
    }

    bool Alive( Handle<Tag> handle ) const
    {
        return handle.mIndex < mSlots.size() and mSlots[handle.mIndex].mGeneration == handle.mGeneration and
               mSlots[handle.mIndex].mValue.has_value();
    }

    OptRef<T> Get( Handle<Tag> handle )
    {
        if ( not Alive( handle ) )
            return std::nullopt;
        return *mSlots[handle.mIndex].mValue;
    }

    OptRef<const T> Get( Handle<Tag> handle ) const
    {
        if ( not Alive( handle ) )
            return std::nullopt;
        return *mSlots[handle.mIndex].mValue;
    }

    size_t Size() const { return mSize; }

    // The live entries by slot. A copy: safe to Add and Remove while
    // going through it.
    vector<Handle<Tag>> Handles() const
    {
        vector<Handle<Tag>> handles;
        handles.reserve( mSize );
        for ( u32 i = 0; i < mSlots.size(); ++i )
            if ( mSlots[i].mValue )
                handles.push_back( Handle<Tag>{ i, mSlots[i].mGeneration } );
        return handles;
    }

private:
    struct Slot
    {
        opt<T> mValue;
        u32 mGeneration = 0;
    };

    // A deque keeps entries in place as it grows.
    std::deque<Slot> mSlots;
    vector<u32> mFree;
    size_t mSize = 0;
};
}

template <typename Tag>
struct std::hash<bubble::Handle<Tag>>
{
    size_t operator()( bubble::Handle<Tag> handle ) const
    {
        return std::hash<bubble::u64>{}( ( bubble::u64( handle.mGeneration ) << 32 ) | handle.mIndex );
    }
};
