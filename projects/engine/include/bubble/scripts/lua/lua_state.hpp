#pragma once
#include "bubble/types/containers.hpp"
#include "bubble/types/number.hpp"
#include "bubble/types/opt_ref.hpp"
#include "bubble/types/pointer.hpp"
#include "bubble/types/string.hpp"

struct lua_State;

namespace bubble
{
class LuaType;

// One Luau VM - a World's, or a test's. The standard libraries are open and
// `print` goes to the log. Engine modules add their globals and types, then
// Seal() makes the globals read-only: from there on every script file runs
// in an environment of its own on top of them (ScriptModule). Where the
// platform can make machine code, functions marked @native and files marked
// --!native run natively.
class LuaState
{
public:
    LuaState();
    ~LuaState();
    LuaState( const LuaState& ) = delete;
    LuaState& operator=( const LuaState& ) = delete;

    lua_State* L() const { return mL; }
    // The LuaState that owns `L` - its main thread or any coroutine of it.
    static LuaState& Of( lua_State* L );

    void Seal();
    bool Sealed() const { return mSealed; }
    bool NativeCode() const { return mNativeCode; }

    // A small number for a name: what field and method lookups on engine
    // types switch on instead of comparing strings. Every name Luau asks
    // about gets one in the order first seen - string constants of loaded
    // scripts too - so engine types take theirs at registration, before
    // scripts can use the space up. -1 once all 32767 are given out.
    i16 Atom( string_view name );
    // Empty for -1 or a number never given out.
    string_view AtomName( i16 atom ) const;

    // Engine types by their userdata tag; LuaTypeBuilder adds them.
    OptRef<LuaType> Type( int tag ) const;

private:
    friend class LuaType;
    friend struct LuaStateAccess;
    static i16 UserAtom( lua_State* L, const char* text, size_t length );

    lua_State* mL = nullptr;
    bool mSealed = false;
    bool mNativeCode = false;
    vector<string> mAtomNames;
    str_hmap<i16> mAtoms;
    vector<Scope<LuaType>> mTypes;
    // The stack where the last error of a PCall was raised.
    string mLastTraceback;
};
}
