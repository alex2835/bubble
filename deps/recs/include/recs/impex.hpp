#pragma once

#ifdef _WIN32
#  ifdef EXPORT_RECS
#     define RECS_EXPORT __declspec(dllexport)
#  elif defined(IMPORT_RECS)
#     define RECS_EXPORT __declspec(dllimport)
#else
#	define RECS_EXPORT
#  endif
#else
#  define RECS_EXPORT __attribute__((visibility("default")))
#endif
