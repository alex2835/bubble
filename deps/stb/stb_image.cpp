// stb_image's code, compiled once: decoding from memory only - the importer
// reads project files through ProjectFiles, never through stdio.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_FAILURE_USERMSG
#include "stb_image.h"
