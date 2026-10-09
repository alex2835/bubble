#pragma once
// How the runtime's error messages name things.
#include "bubble/scripts/lua.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/string.hpp"

namespace bubble::detail
{
// "a, b, c", or "none".
inline string Join( const vector<string>& names )
{
    string list;
    for ( const string& name : names )
        list += ( list.empty() ? "" : ", " ) + name;
    return list.empty() ? "none" : list;
}

// "a number", "a table": a kind as an error message says it.
inline string_view KindName( LuaKind kind )
{
    switch ( kind )
    {
        case LuaKind::Nil: return "nil";
        case LuaKind::Boolean: return "a boolean";
        case LuaKind::Number: return "a number";
        case LuaKind::String: return "a string";
        case LuaKind::Vector: return "a vector";
        case LuaKind::Table: return "a table";
        case LuaKind::Function: return "a function";
        case LuaKind::Userdata: return "a userdata";
        case LuaKind::Thread: return "a thread";
        case LuaKind::Other: break;
    }
    return "a value";
}
}
