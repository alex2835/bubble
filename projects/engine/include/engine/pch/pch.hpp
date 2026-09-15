#pragma once
#include <future>
#include <ranges>
#include <string>

#include "engine/types/glm.hpp"
#include "engine/types/array.hpp"
#include "engine/types/map.hpp"
#include "engine/types/set.hpp"
#include "engine/types/string.hpp"
#include "engine/types/pointer.hpp"
#include "engine/utils/timer.hpp"
#include "engine/utils/filesystem.hpp"
#include "engine/utils/algorihms.hpp"
#include "engine/utils/chrono.hpp"
#include "engine/log/log.hpp"

// Third party headers that nearly every translation unit ends up parsing
// anyway. Together they cost about 2.3 seconds per TU on top of the rest of
// this file; precompiled, the same set costs 0.1. They never change, so they
// never invalidate the PCH.
//
// webgpu.hpp is header-only with its implementation behind
// WEBGPU_CPP_IMPLEMENTATION. The one TU that defines it, webgpu_impl.cpp, is
// excluded from the PCH in CMakeLists - otherwise the forced include would
// bring the header in first, #pragma once would drop the second include, and
// every wgpu:: method would be unresolved at link.
#include <sol/sol.hpp>
#include <nlohmann/json.hpp>
#include <imgui.h>
#include <btBulletDynamicsCommon.h>
#include <magic_enum/magic_enum.hpp>
#include "engine/renderer/webgpu.hpp"
