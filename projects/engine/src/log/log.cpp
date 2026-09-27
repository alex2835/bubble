#include "engine/log/log.hpp"
#include <deque>
#include <fstream>
#include <iostream>
#include <mutex>

namespace bubble
{
namespace
{
// Enough to scroll back through a session; a flood from a script that logs
// every frame drops the oldest rather than growing without end.
constexpr size_t cMaxEntries = 20000;

struct Log
{
    std::mutex mMutex;
    std::deque<LogEntry> mEntries;
    std::uint64_t mNextIndex = 0;
    std::ofstream mFile;
};

Log& TheLog()
{
    static Log log;
    return log;
}

std::string_view Prefix( LogLevel level )
{
    switch ( level )
    {
        case LogLevel::Info: return "[Info] ";
        case LogLevel::Warning: return "[Warning] ";
        case LogLevel::Error: return "[Error] ";
        case LogLevel::Script: return "[Script] ";
    }
    return "";
}
}

void LogMessage( LogLevel level, std::string text )
{
    Log& log = TheLog();
    std::lock_guard lock( log.mMutex );
    std::cout << Prefix( level ) << text << std::endl;
    if ( log.mFile.is_open() )
        log.mFile << Prefix( level ) << text << std::endl;

    log.mEntries.push_back( LogEntry{ level, std::move( text ), log.mNextIndex++ } );
    if ( log.mEntries.size() > cMaxEntries )
        log.mEntries.pop_front();
}

std::uint64_t LogReadSince( std::uint64_t index, std::vector<LogEntry>& out )
{
    Log& log = TheLog();
    std::lock_guard lock( log.mMutex );
    if ( log.mEntries.empty() )
        return log.mNextIndex;
    const std::uint64_t first = log.mEntries.front().mIndex;
    for ( std::uint64_t i = std::max( index, first ); i < log.mNextIndex; i++ )
        out.push_back( log.mEntries[size_t( i - first )] );
    return log.mNextIndex;
}

void LogToFile( const std::filesystem::path& file )
{
    Log& log = TheLog();
    std::lock_guard lock( log.mMutex );
    log.mFile.close();
    if ( not file.empty() )
        log.mFile.open( file, std::ios::trunc );
}

}
