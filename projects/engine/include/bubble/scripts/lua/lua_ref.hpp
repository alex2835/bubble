#pragma once

struct lua_State;

namespace bubble
{
// Holds a Luau value from C++ so the collector keeps it: a module's
// callbacks, an instance's self table. Move-only; a second hold is Copy().
// Must not outlive its LuaState. What it holds is pushed back with Push and
// read on the stack - a LuaRef is a handle, not a C++ copy of the value.
class LuaRef
{
public:
    LuaRef() = default;
    // Holds the value at `index`; nil gives an empty reference.
    LuaRef( lua_State* L, int index );
    ~LuaRef();
    LuaRef( LuaRef&& other ) noexcept;
    LuaRef& operator=( LuaRef&& other ) noexcept;
    LuaRef( const LuaRef& ) = delete;
    LuaRef& operator=( const LuaRef& ) = delete;

    LuaRef Copy() const;
    void Reset();
    bool Empty() const { return mRef <= 0; }

    // Pushes the value, or nil when empty, onto `L` - the state it was
    // taken from or any coroutine of it.
    void Push( lua_State* L ) const;

private:
    // The main thread: a coroutine the value came from may be collected.
    lua_State* mL = nullptr;
    int mRef = -1;
};
}
