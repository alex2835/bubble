#include "bubble/core/assert.hpp"
#include "bubble/core/log.hpp"
#include <cstdlib>

namespace bubble::detail
{
void AssertFailed( const char* condition, const char* message, const char* file, int line )
{
    LogError( "{}:{}: assertion {} failed: {}", file, line, condition, message );
    std::abort();
}
}
