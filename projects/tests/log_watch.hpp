#pragma once
// What was logged while a test ran: the engine's log keeps a history.
#include "bubble/core/log.hpp"
#include <doctest.h>

namespace bubble::test
{
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
