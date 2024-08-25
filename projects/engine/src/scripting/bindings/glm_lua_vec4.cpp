#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>
#include <format>
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include "engine/scripting/bindings/glm_lua_bindings.hpp"

namespace bubble
{
void CreateVec4Bindings( sol::state& lua )
{
    auto vec4_multiply_overloads =
        sol::overload(
            []( const glm::vec4& v1, const glm::vec4& v2 ) { return v1 * v2; },
            []( const glm::vec4& v1, float value ) { return v1 * value; },
            []( float value, const glm::vec4& v1 ) { return v1 * value; }
        );

    auto vec4_divide_overloads =
        sol::overload(
            []( const glm::vec4& v1, const glm::vec4& v2 ) { return v1 / v2; },
            []( const glm::vec4& v1, float value ) { return v1 / value; },
            []( float value, const glm::vec4& v1 ) { return v1 / value; }
        );

    auto vec4_addition_overloads =
        sol::overload(
            []( const glm::vec4& v1, const glm::vec4& v2 ) { return v1 + v2; },
            []( const glm::vec4& v1, float value ) { return v1 + value; },
            []( float value, const glm::vec4& v1 ) { return v1 + value; }
        );

    auto vec4_subtraction_overloads =
        sol::overload(
            []( const glm::vec4& v1, const glm::vec4& v2 ) { return v1 - v2; },
            []( const glm::vec4& v1, float value ) { return v1 - value; },
            []( float value, const glm::vec4& v1 ) { return v1 - value; }
        );

    lua.new_usertype<glm::vec4>(
        "vec4",
        sol::call_constructor,
        sol::constructors<glm::vec4( float ), glm::vec4( float, float, float, float )>(),
        "x",
        &glm::vec4::x,
        "y",
        &glm::vec4::y,
        "z",
        &glm::vec4::z,
        "w",
        &glm::vec4::w,
        sol::meta_function::multiplication,
        vec4_multiply_overloads,
        sol::meta_function::division,
        vec4_divide_overloads,
        sol::meta_function::addition,
        vec4_addition_overloads,
        sol::meta_function::subtraction,
        vec4_subtraction_overloads,
        sol::meta_function::index,
        []( const glm::vec4& v, int i ) { return v[i]; },
        sol::meta_function::new_index,
        []( glm::vec4& v, int i, float value ) { v[i] = value; },
        sol::meta_function::to_string,
        []( const glm::vec4& v ) { return std::format( "[{}, {}, {}, {}]", v.x, v.y, v.z, v.w ); },
        "length",
        []( const glm::vec4& v ) { return glm::length( v ); },
        "lengthSq",
        []( const glm::vec4& v ) { return glm::length2( v ); },
        "normalize",
        []( const glm::vec4& v1 ) { return glm::normalize( v1 ); },
        "normalize2",
        []( const glm::vec4& v1, const glm::vec4& v2 ) { return glm::normalize( v2 - v1 ); }
    );
}

} // namespace bubble
