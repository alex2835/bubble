#pragma once
#include "engine/utils/pointers.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <array>

namespace bubble
{

using namespace glm;

using string = std::string;
template <typename T>
using vector = std::vector<T>;
template <typename T, size_t SIZE>
using array = std::array<T, SIZE>;

using i8 = std::int8_t;
using u8 = std::uint8_t;

using i16 = std::int16_t;
using u16 = std::uint16_t;

using i32 = std::int32_t;
using u32 = std::uint32_t;

using i64 = std::int64_t;
using u64 = std::uint64_t;

using f32 = std::float_t;
using f64 = std::double_t;

}
