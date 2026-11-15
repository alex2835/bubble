#pragma once
#include "bubble/import/importer.hpp"

namespace bubble
{
#ifdef BUBBLE_IMPORT_MODELS
// Models into the engine's meshes, kind "model" (bubble/assets/model_asset.hpp):
// .glb, .gltf, .fbx and .obj read by assimp, their nodes' transforms put into
// the vertices, a mesh a material. Each mesh ordered for the GPU's caches by
// meshoptimizer - vertices, overdraw, fetching - and simplified into the
// levels of detail asked for, every level sharing the mesh's vertices. The
// settings, every one may be left out:
//
//     { "lods": [0.5, 0.25],   each level's share of the full mesh's triangles;
//                              a level the simplifier cannot reach ends them
//       "scale": 1 }           what positions are multiplied by
//
// A model is read from its own file alone, for now: a .gltf with its buffers
// in other files is not read - a .glb carries them inside. A model that will
// not read is refused with assimp's reason.
//
// Not on the web, where neither the editor nor the cook runs.
vector<Importer> ModelImporters();
#endif
}
