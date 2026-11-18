#pragma once
// A small skinned model made in code, as a .glb - no model files in the
// repository: a strip two units high on two joints, "lower" at the origin
// and "upper" one up, under an "armature" node; its bottom row bound to
// lower, its middle half to each, its top to upper. Three clips of a
// second: "bend" turns upper a quarter about Z, "lean" turns lower a quarter
// the other way, "walk" moves lower one along X.
//
// Z-up, the armature stands as an FBX's does - turned a quarter about X, its
// own up Z - and the model is the same; a fourth clip, "turn", turns lower a
// quarter about that up while leaning it 30 degrees about the armature's X.
#include "bubble/types/containers.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/number.hpp"
#include "bubble/types/string.hpp"
#include <cstring>

namespace bubble::test
{
inline string SkinnedGlb( bool zUp = false )
{
    string bin;
    vector<string> views;
    // A block of the binary buffer, as a buffer view: its offset and length.
    const auto block = [&]( const auto& values ) {
        const size_t offset = bin.size();
        const size_t bytes = sizeof( values[0] ) * values.size();
        bin.resize( offset + bytes );
        std::memcpy( bin.data() + offset, values.data(), bytes );
        views.push_back( format( R"({{ "buffer": 0, "byteOffset": {}, "byteLength": {} }})", offset, bytes ) );
        return views.size() - 1;
    };
    const f32 h = 0.70710677f;
    const size_t positions =
        block( vector<f32>{ -0.1f, 0, 0, 0.1f, 0, 0, -0.1f, 1, 0, 0.1f, 1, 0, -0.1f, 2, 0, 0.1f, 2, 0 } );
    const size_t normals = block( vector<f32>{ 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1 } );
    const size_t joints =
        block( vector<u16>{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 } );
    const size_t weights =
        block( vector<f32>{ 1, 0, 0, 0, 1, 0, 0, 0, 0.5f, 0.5f, 0, 0, 0.5f, 0.5f, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 } );
    const size_t indices = block( vector<u16>{ 0, 1, 3, 0, 3, 2, 2, 3, 5, 2, 5, 4 } );
    // Column by column: the joints' binds undone, the armature's turn too.
    const size_t inverseBinds = zUp ? block( vector<f32>{ 1, 0, 0, 0, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0,  1,
                                                          1, 0, 0, 0, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, -1, 1 } )
                                    : block( vector<f32>{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0,  0, 1,
                                                          1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, -1, 0, 1 } );
    const size_t times = block( vector<f32>{ 0, 1 } );
    const size_t bend = block( vector<f32>{ 0, 0, 0, 1, 0, 0, h, h } );
    const size_t lean = block( vector<f32>{ 0, 0, 0, 1, 0, 0, -h, h } );
    const size_t walk = block( vector<f32>{ 0, 0, 0, 1, 0, 0 } );
    // A quarter about Z after 30 degrees about X: the half angles' sines and
    // cosines multiplied out.
    const f32 hs = h * 0.25881904f;
    const f32 hc = h * 0.9659258f;
    const size_t turn = block( vector<f32>{ 0, 0, 0, 1, hs, hs, hc, hc } );

    string viewList;
    for ( const string& view : views )
        viewList += ( viewList.empty() ? "" : ", " ) + view;
    const string json = format(
        R"({{ "asset": {{ "version": "2.0" }}, "scene": 0, "scenes": [ {{ "nodes": [ 0 ] }} ],
  "nodes": [ {{ "name": "armature", "children": [ 1, 3 ]{} }}, {{ "name": "lower", "children": [ 2 ] }},
             {{ "name": "upper", "translation": {} }}, {{ "name": "strip", "mesh": 0, "skin": 0 }} ],
  "meshes": [ {{ "primitives": [ {{ "attributes": {{ "POSITION": 0, "NORMAL": 1, "JOINTS_0": 2, "WEIGHTS_0": 3 }},
                                   "indices": 4, "material": 0 }} ] }} ],
  "materials": [ {{ "name": "skin" }} ],
  "skins": [ {{ "joints": [ 1, 2 ], "inverseBindMatrices": 5 }} ],
  "animations": [
    {{ "name": "bend", "samplers": [ {{ "input": 6, "output": 7 }} ],
       "channels": [ {{ "sampler": 0, "target": {{ "node": 2, "path": "rotation" }} }} ] }},
    {{ "name": "lean", "samplers": [ {{ "input": 6, "output": 8 }} ],
       "channels": [ {{ "sampler": 0, "target": {{ "node": 1, "path": "rotation" }} }} ] }},
    {{ "name": "walk", "samplers": [ {{ "input": 6, "output": 9 }} ],
       "channels": [ {{ "sampler": 0, "target": {{ "node": 1, "path": "translation" }} }} ] }}{} ],
  "buffers": [ {{ "byteLength": {} }} ],
  "bufferViews": [ {} ],
  "accessors": [
    {{ "bufferView": {}, "componentType": 5126, "type": "VEC3", "count": 6, "min": [ -0.1, 0, 0 ], "max": [ 0.1, 2, 0 ] }},
    {{ "bufferView": {}, "componentType": 5126, "type": "VEC3", "count": 6 }},
    {{ "bufferView": {}, "componentType": 5123, "type": "VEC4", "count": 6 }},
    {{ "bufferView": {}, "componentType": 5126, "type": "VEC4", "count": 6 }},
    {{ "bufferView": {}, "componentType": 5123, "type": "SCALAR", "count": 12 }},
    {{ "bufferView": {}, "componentType": 5126, "type": "MAT4", "count": 2 }},
    {{ "bufferView": {}, "componentType": 5126, "type": "SCALAR", "count": 2, "min": [ 0 ], "max": [ 1 ] }},
    {{ "bufferView": {}, "componentType": 5126, "type": "VEC4", "count": 2 }},
    {{ "bufferView": {}, "componentType": 5126, "type": "VEC4", "count": 2 }},
    {{ "bufferView": {}, "componentType": 5126, "type": "VEC3", "count": 2 }},
    {{ "bufferView": {}, "componentType": 5126, "type": "VEC4", "count": 2 }} ] }})",
        zUp ? R"(, "rotation": [ -0.70710677, 0, 0, 0.70710677 ])" : "", zUp ? "[ 0, 0, 1 ]" : "[ 0, 1, 0 ]",
        zUp ? R"(,
    { "name": "turn", "samplers": [ { "input": 6, "output": 10 } ],
      "channels": [ { "sampler": 0, "target": { "node": 1, "path": "rotation" } } ] })"
            : "",
        bin.size(), viewList, positions, normals, joints, weights, indices, inverseBinds, times, bend, lean, walk,
        turn );

    // The container: a header, then the JSON and the binary, each padded to
    // four bytes.
    string text = json;
    text.resize( ( text.size() + 3 ) / 4 * 4, ' ' );
    bin.resize( ( bin.size() + 3 ) / 4 * 4, '\0' );
    string glb;
    const auto put = [&]( u32 value ) { glb.append( reinterpret_cast<const char*>( &value ), 4 ); };
    put( 0x46546C67 );
    put( 2 );
    put( static_cast<u32>( 12 + 8 + text.size() + 8 + bin.size() ) );
    put( static_cast<u32>( text.size() ) );
    put( 0x4E4F534A );
    glb += text;
    put( static_cast<u32>( bin.size() ) );
    put( 0x004E4942 );
    glb += bin;
    return glb;
}
}
