#pragma once
#include "bubble/types/containers.hpp"
#include "bubble/types/number.hpp"
#include "bubble/types/string.hpp"
#include <filesystem>
#include <format>

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
void LogToFile( const std::filesystem::path& file );

// Messages go nowhere but the history: for tests that check what was logged
// without filling the output.
void LogMuteOutput( bool mute );

template <typename... Args>
void Log( LogLevel level, std::format_string<Args...> format, Args&&... args )
{
    LogMessage( level, std::format( format, std::forward<Args>( args )... ) );
}

template <typename... Args>
void LogInfo( std::format_string<Args...> format, Args&&... args )
{
    Log( LogLevel::Info, format, std::forward<Args>( args )... );
}

template <typename... Args>
void LogWarning( std::format_string<Args...> format, Args&&... args )
{
    Log( LogLevel::Warning, format, std::forward<Args>( args )... );
}

template <typename... Args>
void LogError( std::format_string<Args...> format, Args&&... args )
{
    Log( LogLevel::Error, format, std::forward<Args>( args )... );
}
}
