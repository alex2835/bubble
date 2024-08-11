#pragma once

#ifdef WIN32
#  ifdef EXPORT_RECS
#     define RECS_EXPORT __declspec(dllexport)
#  elif IMPORT_RECS
#     define RECS_EXPORT __declspec(dllimport)
#else
#	define RECS_EXPORT
#  endif
#else
#  define RECS_EXPORT __attribute__((visibility("default")))
#endif
