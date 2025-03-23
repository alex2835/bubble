#include <sol/sol.hpp>
#include <format>
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include "glm_lua_bindings.hpp"

namespace bubble
{
void CreateVec2Bindings( sol::state& lua )
{
    auto vec2_multiply_overloads =
        sol::overload(
            []( const glm::vec2& v1, const glm::vec2& v2 ) { return v1 * v2; },
            []( const glm::vec2& v1, float value ) { return v1 * value; },
            []( float value, const glm::vec2& v1 ) { return v1 * value; }
        );

    auto vec2_divide_overloads =
        sol::overload( []( const glm::vec2& v1, const glm::vec2& v2 ) { return v1 / v2; },
                       []( const glm::vec2& v1, float value ) { return v1 / value; },
                       []( float value, const glm::vec2& v1 ) { return v1 / value; }
        );

    auto vec2_addition_overloads =
        sol::overload( []( const glm::vec2& v1, const glm::vec2& v2 ) { return v1 + v2; },
                       []( const glm::vec2& v1, float value ) { return v1 + value; },
                       []( float value, const glm::vec2& v1 ) { return v1 + value; }
        );

    auto vec2_subtraction_overloads =
        sol::overload( []( const glm::vec2& v1, const glm::vec2& v2 ) { return v1 - v2; },
                       []( const glm::vec2& v1, float value ) { return v1 - value; },
                       []( float value, const glm::vec2& v1 ) { return v1 - value; }
        );

    lua.new_usertype<glm::vec2>(
        "vec2",
        sol::call_constructor,
        sol::constructors<glm::vec2(), glm::vec2( float ), glm::vec2( float, float )>(),
        "x",
        &glm::vec2::x,
        "y",
        &glm::vec2::y,
        sol::meta_function::multiplication,
        vec2_multiply_overloads,
        sol::meta_function::division,
        vec2_divide_overloads,
        sol::meta_function::addition,
        vec2_addition_overloads,
        sol::meta_function::subtraction,
        vec2_subtraction_overloads,
        sol::meta_function::index,
        []( const glm::vec2& v, int i ) { return v[i]; },
        sol::meta_function::new_index,
        []( glm::vec2& v, int i, float value ) { v[i] = value; },
        sol::meta_function::to_string,
        []( const glm::vec2& v ) { return std::format( "[{}, {}]", v.x, v.y ); },
        "length",
        []( const glm::vec2& v ) { return glm::length( v ); },
        "lengthSq",
        []( const glm::vec2& v ) { return glm::length2( v ); },
        "normalize",
        []( const glm::vec2& v1 ) { return glm::normalize( v1 ); },
        "normalize2",
        []( const glm::vec2& v1, const glm::vec2& v2 ) { return glm::normalize( v2 - v1 ); }
    );
}

} // namespace bubble

