#pragma once

#ifdef WIN32
#  ifdef BUILDING_BUBBLE_ENGINE
#     define BUBBLE_ENGINE_DLL_EXPORT __declspec(dllexport)
#  else
#     define BUBBLE_ENGINE_DLL_EXPORT __declspec(dllimport)
#  endif
#else
#  define FEATURE_CACHE_IMPEXP __attribute__((visibility("default")))
#endif
