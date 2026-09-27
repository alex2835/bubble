#pragma once
#include <format>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <cstdint>

// The engine's log. Every message goes three ways: to stdout, to a log file
// when one is open (the editor opens one: it has no terminal), and into an
// in-memory history that the editor's Console window reads. Safe to call
// from any thread.
namespace bubble
{
enum class LogLevel
{
    Info,
    Warning,
    Error,
    // What scripts print: game scripts and the editor's console alike.
    Script,
};

struct LogEntry
{
    LogLevel mLevel = LogLevel::Info;
    std::string mText;
    // Counts every message ever logged, dropped ones too: what a reader
    // asks for more from.
    std::uint64_t mIndex = 0;
};

void LogMessage( LogLevel level, std::string text );

// Copies out the entries from `index` on that are still kept - the last few
// thousand - and returns the index to ask from next.
std::uint64_t LogReadSince( std::uint64_t index, std::vector<LogEntry>& out );
// Also writes every message to `file`, truncated first. Empty stops it.
void LogToFile( const std::filesystem::path& file );

template <typename ...Args>
void LogError( std::string_view format, const Args& ...args )
{
    if constexpr ( sizeof...( Args ) == 0 )
        LogMessage( LogLevel::Error, std::string( format ) );
    else
        LogMessage( LogLevel::Error, std::vformat( format, std::make_format_args( args... ) ) );
}

template <typename ...Args>
void LogWarning( std::string_view format, const Args& ...args )
{
    if constexpr ( sizeof...( Args ) == 0 )
        LogMessage( LogLevel::Warning, std::string( format ) );
    else
        LogMessage( LogLevel::Warning, std::vformat( format, std::make_format_args( args... ) ) );
}

template <typename ...Args>
void LogInfo( std::string_view format, const Args& ...args )
{
    if constexpr ( sizeof...( Args ) == 0 )
        LogMessage( LogLevel::Info, std::string( format ) );
    else
        LogMessage( LogLevel::Info, std::vformat( format, std::make_format_args( args... ) ) );
}

}
