#pragma once
#include "bubble/scene/scene.hpp"
#include "bubble/types/string.hpp"

namespace bubble
{
// The editor's icons: characters of Lucide's font, merged into the editor's
// (editor_font.hpp), so an icon is UTF-8 in a label like any letter. The
// codepoints are Lucide 1.66.0's, as deps/lucide says.
namespace Icon
{
inline constexpr const char* cSun = "\xEE\x85\xB8";        // sun
inline constexpr const char* cLightbulb = "\xEE\x87\x82";  // lightbulb
inline constexpr const char* cFlashlight = "\xEE\x83\x93"; // flashlight
inline constexpr const char* cVideo = "\xEE\x86\xA5";      // video
inline constexpr const char* cVolume = "\xEE\x86\xAB";     // volume-2
inline constexpr const char* cEar = "\xEE\x8E\x82";        // ear
inline constexpr const char* cBox = "\xEE\x81\xA1";        // box
inline constexpr const char* cPerson = "\xEE\x88\x9E";     // person-standing
inline constexpr const char* cCollider = "\xEE\x87\x8B";   // square-dashed
inline constexpr const char* cCanvas = "\xEE\x87\x81";     // layout-dashboard
inline constexpr const char* cRect = "\xEE\x85\xA7";       // square
inline constexpr const char* cText = "\xEE\x86\x98";       // type
inline constexpr const char* cButton = "\xEE\x84\xA0";     // mouse-pointer-click
inline constexpr const char* cImage = "\xEE\x83\xB6";      // image
inline constexpr const char* cSlider = "\xEE\x8A\x9A";     // sliders-horizontal
inline constexpr const char* cToggle = "\xEE\x86\x8C";     // toggle-right
inline constexpr const char* cScript = "\xEE\x83\x83";     // file-code
inline constexpr const char* cPrefab = "\xEE\x84\xA9";     // package
inline constexpr const char* cBone = "\xEE\x8D\x98";       // bone
inline constexpr const char* cFolder = "\xEE\x83\x97";     // folder
inline constexpr const char* cEmpty = "\xEE\x8B\xBE";      // axis-3d
inline constexpr const char* cAnimator = "\xEE\x83\x90";   // film
// The project's files, by kind, and how the assets panel shows them.
inline constexpr const char* cMaterial = "\xEE\x87\x9D"; // palette
inline constexpr const char* cSound = "\xEE\x95\x9A";    // audio-lines
inline constexpr const char* cFont = "\xEE\x86\x98";     // type
inline constexpr const char* cLevel = "\xEE\x84\x90";    // map
inline constexpr const char* cShader = "\xEE\x90\x92";   // sparkles
inline constexpr const char* cGraph = "\xEE\x90\xA5";    // workflow
inline constexpr const char* cFile = "\xEE\x83\x80";     // file
inline constexpr const char* cTiles = "\xEE\x83\xBF";    // layout-grid
inline constexpr const char* cList = "\xEE\x84\x86";     // list
}

// What an entity is, as an icon for its row in the scene's tree: by its
// components, the most telling first - a prefab's instance, a camera, a
// light, a character, the UI, a model, a sound, physics, a script -; with
// none of them a folder when it holds others, a bare transform else.
string_view IconOf( const Scene& scene, Entity entity );
// The icon the viewport marks an entity with where it stands: what the
// picture does not show - a light, a camera, a sound, the listener - and
// nothing has a model to show it by.
opt<string_view> TryMarkerOf( const Scene& scene, Entity entity );
// Every entity of the scene TryMarkerOf marks, each once.
vector<Entity> MarkedEntities( const Scene& scene );
}
