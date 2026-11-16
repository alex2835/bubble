#pragma once
// What the script tests share: compiling a snippet, and reading back what
// was logged.
#include "bubble/scripts/lua.hpp"
#include "log_watch.hpp"
#include <doctest.h>
#include <lua.h>

namespace bubble::test
{
inline string Bytecode( string_view source )
{
    auto bytecodeMaybe = TryCompileScript( source );
    REQUIRE_MESSAGE( bytecodeMaybe.has_value(), bytecodeMaybe.error() );
    return *bytecodeMaybe;
}

}
