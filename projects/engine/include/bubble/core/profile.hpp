#pragma once
// Profiler zones. With BUBBLE_PROFILE on they go to Tracy - run the Tracy
// profiler (same version as deps/tracy) and connect; without it every macro
// is nothing at all. Modules use these, never Tracy directly.
//
//   void World::Frame()
//   {
//       BUBBLE_PROFILE_ZONE();                    // named after the function
//       { BUBBLE_PROFILE_ZONE_NAMED( "physics" ); ... }
//       BUBBLE_PROFILE_PLOT( "draw calls", n );
//       BUBBLE_PROFILE_FRAME();                   // once per frame
//   }

#ifdef BUBBLE_PROFILE_ENABLED

#include <tracy/Tracy.hpp>

#define BUBBLE_PROFILE_ZONE() ZoneScoped
#define BUBBLE_PROFILE_ZONE_NAMED( name ) ZoneScopedN( name )
#define BUBBLE_PROFILE_FRAME() FrameMark
#define BUBBLE_PROFILE_PLOT( name, value ) TracyPlot( name, value )
// A line on the timeline; `text` is copied.
#define BUBBLE_PROFILE_MESSAGE( text, size ) TracyMessage( text, size )
#define BUBBLE_PROFILE_THREAD_NAME( name ) tracy::SetThreadName( name )

#else

#define BUBBLE_PROFILE_ZONE() static_cast<void>( 0 )
#define BUBBLE_PROFILE_ZONE_NAMED( name ) static_cast<void>( 0 )
#define BUBBLE_PROFILE_FRAME() static_cast<void>( 0 )
#define BUBBLE_PROFILE_PLOT( name, value ) static_cast<void>( 0 )
#define BUBBLE_PROFILE_MESSAGE( text, size ) static_cast<void>( 0 )
#define BUBBLE_PROFILE_THREAD_NAME( name ) static_cast<void>( 0 )

#endif
