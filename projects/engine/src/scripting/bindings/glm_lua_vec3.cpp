#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>
#include <format>
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include "engine/scripting/bindings/glm_lua_bindings.hpp"

namespace bubble
{
void CreateVec3Bindings( sol::state& lua )
{
    auto vec3_multiply_overloads =
        sol::overload(
            []( const glm::vec3& v1, const glm::vec3& v2 ) { return v1 * v2; },
            []( const glm::vec3& v1, float value ) { return v1 * value; },
            []( float value, const glm::vec3& v1 ) { return v1 * value; }
        );

    auto vec3_divide_overloads =
        sol::overload(
            []( const glm::vec3& v1, const glm::vec3& v2 ) { return v1 / v2; },
            []( const glm::vec3& v1, float value ) { return v1 / value; },
            []( float value, const glm::vec3& v1 ) { return v1 / value; }
        );

    auto vec3_addition_overloads =
        sol::overload(
            []( const glm::vec3& v1, const glm::vec3& v2 ) { return v1 + v2; },
            []( const glm::vec3& v1, float value ) { return v1 + value; },
            []( float value, const glm::vec3& v1 ) { return v1 + value; }
        );

    auto vec3_subtraction_overloads =
        sol::overload(
            []( const glm::vec3& v1, const glm::vec3& v2 ) { return v1 - v2; },
            []( const glm::vec3& v1, float value ) { return v1 - value; },
            []( float value, const glm::vec3& v1 ) { return v1 - value; }
        );

    lua.new_usertype<glm::vec3>(
        "vec3",
        sol::call_constructor,
        sol::constructors<glm::vec3(), glm::vec3( float ), glm::vec3( float, float, float )>(),
        "x",
        &glm::vec3::x,
        "y",
        &glm::vec3::y,
        "z",
        &glm::vec3::z,
        sol::meta_function::multiplication,
        vec3_multiply_overloads,
        sol::meta_function::division,
        vec3_divide_overloads,
        sol::meta_function::addition,
        vec3_addition_overloads,
        sol::meta_function::subtraction,
        vec3_subtraction_overloads,
        sol::meta_function::index,
        []( const glm::vec3& v, int i ) { return v[i]; },
        sol::meta_function::new_index,
        []( glm::vec3& v, int i, float value ) { v[i] = value; },
        sol::meta_function::to_string,
        []( const glm::vec3& v ) { return std::format( "[{}, {}, {}]", v.x, v.y, v.z ); },
        "length",
        []( const glm::vec3& v ) { return glm::length( v ); },
        "lengthSq",
        []( const glm::vec3& v ) { return glm::length2( v ); },
        "normalize",
        []( const glm::vec3& v1 ) { return glm::normalize( v1 ); },
        "normalize2",
        []( const glm::vec3& v1, const glm::vec3& v2 ) { return glm::normalize( v2 - v1 ); }
    );
}

} // namespace bubble

