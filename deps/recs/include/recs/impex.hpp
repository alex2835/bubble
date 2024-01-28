#pragma once

#ifdef RECS_STATIC
#  define RECS_EXPORT
#else
#  ifdef WIN32
#    ifdef BUILDING_RECS
#       define RECS_EXPORT __declspec(dllexport)
#    else
#       define RECS_EXPORT __declspec(dllimport)
#    endif
#  else
#    define RECS_EXPORT __attribute__((visibility("default")))
#  endif
#endif
