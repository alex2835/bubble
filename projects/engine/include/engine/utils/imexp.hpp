#pragma once

#ifdef BUBBLE_STATIC
#  define BUBBLE_ENGINE_EXPORT
#else
#  ifdef WIN32
#    ifdef EXPORT_BUBBLE_ENGINE
#       define BUBBLE_ENGINE_EXPORT __declspec(dllexport)
#	 elif IMPORT_BUBBLE_ENGINE
#       define BUBBLE_ENGINE_EXPORT __declspec(dllimport)
#    else
#       define BUBBLE_ENGINE_EXPORT
#    endif
#  else
#    define BUBBLE_ENGINE_EXPORT __attribute__((visibility("default")))
#  endif
#endif
