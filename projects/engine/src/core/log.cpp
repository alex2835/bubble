#include "bubble/core/log.hpp"
#include "bubble/core/profile.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/stream.hpp"
#include "bubble/types/thread.hpp"

namespace bubble
{
namespace
{
// Enough to scroll back through a session; a script that logs every frame
// drops the oldest rather than growing without end.
constexpr size_t cMaxEntries = 20000;

struct LogState
{
    mutex mMutex;
    deque<LogEntry> mEntries;
    u64 mNextIndex = 0;
    ofstream mFile;
    bool mMuted = false;
};

LogState& State()
{
    static LogState state;
    return state;
}

string_view Prefix( LogLevel level )
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

void LogMessage( LogLevel level, string text )
{
    LogState& state = State();
    lock_guard lock( state.mMutex );
    if ( not state.mMuted )
        cout << Prefix( level ) << text << endl;
    if ( state.mFile.is_open() )
        state.mFile << Prefix( level ) << text << endl;
    // On the profiler's timeline too, beside the frame it happened in.
    BUBBLE_PROFILE_MESSAGE( text.data(), text.size() );

    state.mEntries.push_back( LogEntry{ level, std::move( text ), state.mNextIndex++ } );
    if ( state.mEntries.size() > cMaxEntries )
        state.mEntries.pop_front();
}

u64 LogReadSince( u64 index, vector<LogEntry>& out )
{
    LogState& state = State();
    lock_guard lock( state.mMutex );
    if ( state.mEntries.empty() )
        return state.mNextIndex;
    const u64 first = state.mEntries.front().mIndex;
    for ( u64 i = max( index, first ); i < state.mNextIndex; i++ )
        out.push_back( state.mEntries[size_t( i - first )] );
    return state.mNextIndex;
}

void LogToFile( const OsPath& file )
{
    LogState& state = State();
    lock_guard lock( state.mMutex );
    state.mFile.close();
    if ( not file.empty() )
        state.mFile.open( file, ios::trunc );
}

void LogMuteOutput( bool mute )
{
    LogState& state = State();
    lock_guard lock( state.mMutex );
    state.mMuted = mute;
}
}
