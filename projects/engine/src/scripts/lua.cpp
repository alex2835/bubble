#include "bubble/scripts/lua.hpp"
#include "bubble/core/log.hpp"

namespace bubble
{
void PrintToLog( string_view text )
{
    LogMessage( LogLevel::Script, string( text ) );
}
}
