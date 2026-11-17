#pragma once
#include "bubble/import/importer.hpp"

namespace bubble
{
// .anim, kind "animation_graph": the file as it is, once it reads as an
// animation graph (bubble/animation/animation_graph.hpp). Its clips are
// names the animator's model is asked for when it plays, not checked here.
Importer AnimationGraphImporter();
}
