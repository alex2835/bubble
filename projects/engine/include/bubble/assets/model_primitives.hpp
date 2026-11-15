#pragma once
#include "bubble/assets/model_asset.hpp"

namespace bubble
{
// Models made in code rather than imported: what the editor offers to add,
// what tests and the runner draw. One mesh, its full level alone; faces
// turn counter-clockwise seen from outside, as imported ones do.

// A box `size` across, centred on the origin; each face flat, its uvs over
// the whole image.
ModelAsset MakeBox( vec3 size );
// A square `size` across in X and Z, facing +Y.
ModelAsset MakePlane( f32 size );
}
