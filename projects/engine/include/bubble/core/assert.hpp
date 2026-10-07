#pragma once

// A check of the engine's own invariant, in debug builds only: for what is
// too costly to check in every build or sits on a hot path. A failed one is
// a bug in the engine - it logs the condition and where, and stops the
// program. What a caller of an API can get wrong is a logic_error
// instead, checked in every build.
#ifdef NDEBUG
// Not evaluated, but still compiled: a variable used only here is used.
#define BUBBLE_ASSERT( condition, message ) static_cast<void>( sizeof( ( condition ) ? 1 : 0 ) )
#else
#define BUBBLE_ASSERT( condition, message )                                                                            \
    ( ( condition ) ? static_cast<void>( 0 )                                                                           \
                    : ::bubble::detail::AssertFailed( #condition, message, __FILE__, __LINE__ ) )
#endif

namespace bubble::detail
{
[[noreturn]] void AssertFailed( const char* condition, const char* message, const char* file, int line );
}
