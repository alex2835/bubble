#pragma once
#include "bubble/types/containers.hpp"
#include "bubble/types/filesystem.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/number.hpp"
#include "bubble/types/string.hpp"

// The engine's log - one of the two globals the engine has (the profiler is
// the other). Every message goes to stdout, to a log file when one is open,
// and into an in-memory history the editor's console reads. Safe to call
// from any thread.
namespace bubble
{
enum class LogLevel
{
    Info,
    Warning,
    Error,
    // What scripts print.
    Script,
};

struct LogEntry
{
    LogLevel mLevel = LogLevel::Info;
    string mText;
    // Counts every message ever logged, dropped ones too: what a reader asks
    // for more from.
    u64 mIndex = 0;
};

void LogMessage( LogLevel level, string text );

// Copies out the entries from `index` on that are still kept - the last
// few thousand - and returns the index to ask from next.
u64 LogReadSince( u64 index, vector<LogEntry>& out );

// Also writes every message to `file`, truncated first. Empty stops it.
void LogToFile( const OsPath& file );

// Messages go nowhere but the history: for tests that check what was logged
// without filling the output.
void LogMuteOutput( bool mute );

template <typename... Args>
void Log( LogLevel level, format_string<Args...> pattern, Args&&... args )
{
    LogMessage( level, format( pattern, std::forward<Args>( args )... ) );
}

template <typename... Args>
void LogInfo( format_string<Args...> pattern, Args&&... args )
{
    Log( LogLevel::Info, pattern, std::forward<Args>( args )... );
}

template <typename... Args>
void LogWarning( format_string<Args...> pattern, Args&&... args )
{
    Log( LogLevel::Warning, pattern, std::forward<Args>( args )... );
}

template <typename... Args>
void LogError( format_string<Args...> pattern, Args&&... args )
{
    Log( LogLevel::Error, pattern, std::forward<Args>( args )... );
}
}
