#pragma once

namespace bubble
{
// What every executable of the engine does first: the console shows UTF-8
// (on Windows it would otherwise show the OEM code page). Together with the
// manifest every executable carries (bubble_executable in CMake), which makes
// the process's ANSI code page UTF-8, char strings mean UTF-8 everywhere.
void InitProcess();

// The process's ANSI code page is UTF-8 - always on platforms other than
// Windows; on Windows only when the manifest took effect.
bool ProcessUsesUtf8();
}
