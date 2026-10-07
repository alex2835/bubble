#pragma once
// What the script tests share: compiling a snippet, and reading back what
// was logged.
#include "bubble/core/log.hpp"
#include "bubble/scripts/lua.hpp"
#include <doctest.h>
#include <lua.h>

namespace bubble::test
{
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
    bool Saw( LogLevel level, initializer_list<string_view> parts ) const
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
