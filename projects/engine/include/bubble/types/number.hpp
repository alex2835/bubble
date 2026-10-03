#pragma once
#include <cstdint>

namespace bubble
{
using i8 = std::int8_t;
using u8 = std::uint8_t;
using i16 = std::int16_t;
using u16 = std::uint16_t;
using i32 = std::int32_t;
using u32 = std::uint32_t;
using i64 = std::int64_t;
using u64 = std::uint64_t;
// Exactly float and double: std::float_t may be wider where the platform
// evaluates floats in a wider type.
using f32 = float;
using f64 = double;

static_assert( sizeof( f32 ) == 4 and sizeof( f64 ) == 8 );
}
