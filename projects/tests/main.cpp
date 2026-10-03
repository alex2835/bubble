#define DOCTEST_CONFIG_IMPLEMENT
#include "bubble/core/log.hpp"
#include <doctest.h>

int main( int argc, char** argv )
{
    // Tests check the history; the console stays readable.
    bubble::LogMuteOutput( true );
    doctest::Context context( argc, argv );
    return context.run();
}
