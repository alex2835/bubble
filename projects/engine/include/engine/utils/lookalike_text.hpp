#pragma once
#include "engine/types/string.hpp"

// Text that reads the same to a person but is not the same string: a
// Cyrillic 'es' typed for a Latin 'c', "Camera" for "camera". For error
// messages that should point at the likely typo. UTF-8 in, UTF-8 out.
namespace bubble
{
// Whether `a` and `b` look the same: equal once ASCII is lowercased and the
// Cyrillic letters drawn like Latin ones are taken as those.
bool LooksAlike( string_view a, string_view b );

// The letters of `text` that are not ASCII, with their code points, to show
// what makes two lookalikes differ: each as "'<letter>' (U+0441)", joined
// with ", ". Empty when it is all ASCII.
string NonAsciiLetters( string_view text );

}
