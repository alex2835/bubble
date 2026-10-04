#pragma once
// What the script tests share: compiling and running a snippet, and reading
// back what was logged.
#include "bubble/core/log.hpp"
#include "bubble/scripts/lua/lua_call.hpp"
#include "bubble/scripts/lua/lua_state.hpp"
#include <doctest.h>
#include <lua.h>

namespace bubble::test
{
// Compiles and runs `source` in L's globals; on success its results stay on
// the stack, else the error comes back.
inline expected<void, ScriptError> RunLua( lua_State* L, string_view source, int results = 0 )
{
    auto bytecode = CompileScript( source );
    if ( not bytecode )
        return std::unexpected( ScriptError{ bytecode.error(), {} } );
    if ( auto loaded = LoadScript( L, "=test", *bytecode ); not loaded )
        return std::unexpected( ScriptError{ loaded.error(), {} } );
    return PCall( L, 0, results );
}

inline string Bytecode( string_view source )
{
    auto bytecode = CompileScript( source );
    REQUIRE_MESSAGE( bytecode.has_value(), bytecode.error() );
    return *bytecode;
}

// The messages logged from here on.
class LogWatch
{
public:
    LogWatch()
    {
        vector<LogEntry> ignored;
        mFrom = LogReadSince( 0, ignored );
    }

    vector<LogEntry> Entries() const
    {
        vector<LogEntry> entries;
        LogReadSince( mFrom, entries );
        return entries;
    }

    // Some message of `level` contains every one of `parts`.
    bool Saw( LogLevel level, std::initializer_list<string_view> parts ) const
    {
        for ( const LogEntry& entry : Entries() )
        {
            if ( entry.mLevel != level )
                continue;
            bool all = true;
            for ( string_view part : parts )
                all = all and entry.mText.find( part ) != string::npos;
            if ( all )
                return true;
        }
        return false;
    }

private:
    u64 mFrom = 0;
};
}
