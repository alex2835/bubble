#include <sol/sol.hpp>
#include <glm/glm.hpp>
#include <format>
#include "glm_lua_bindings.hpp"

namespace bubble
{
void CreateMat2Bindings( sol::state& lua )
{
    auto mat2_multiply_overloads =
        sol::overload(
            []( const glm::mat2& m1, const glm::mat2& m2 ) { return m1 * m2; },
            []( const glm::mat2& m, const glm::vec2& v ) { return m * v; },
            []( const glm::mat2& m, float value ) { return m * value; },
            []( float value, const glm::mat2& m ) { return m * value; }
        );

    auto mat2_addition_overloads = []( const glm::mat2& m1, const glm::mat2& m2 ) { return m1 + m2; };

    auto mat2_subtraction_overloads = []( const glm::mat2& m1, const glm::mat2& m2 ) { return m1 - m2; };

    auto mat2_to_string = []( const glm::mat2& m )
    {
        return std::format( "[{}, {},\n"
                            " {}, {}]",
                            m[0][0], m[0][1],
                            m[1][0], m[1][1] );
    };

    lua.new_usertype<glm::mat2>(
        "mat2",
        sol::call_constructor,
        sol::constructors<glm::mat2()>(),
        sol::meta_function::multiplication,
        mat2_multiply_overloads,
        sol::meta_function::addition,
        mat2_addition_overloads,
        sol::meta_function::subtraction,
        mat2_subtraction_overloads,
        sol::meta_function::index,
        []( glm::mat2& m, int i ) -> glm::vec2& { return m[i]; },
        sol::meta_function::to_string,
        mat2_to_string
    );
}


void CreateMat3Bindings( sol::state& lua )
{
    auto mat3_multiply_overloads =
        sol::overload(
            []( const glm::mat3& m1, const glm::mat3& m2 ) { return m1 * m2; },
            []( const glm::mat3& m, const glm::vec3& v ) { return m * v; },
            []( const glm::mat3& m, float value ) { return m * value; },
            []( float value, const glm::mat3& m ) { return m * value; }
        );

    auto mat3_addition_overloads = []( const glm::mat3& m1, const glm::mat3& m2 ) { return m1 + m2; };

    auto mat3_subtraction_overloads = []( const glm::mat3& m1, const glm::mat3& m2 ) { return m1 - m2; };

    auto mat3_to_string = []( const glm::mat3& m )
    {
        return std::format( "[{}, {}, {},\n"
                            " {}, {}, {},\n"
                            " {}, {}, {}]",
                            m[0][0], m[0][1], m[0][2],
                            m[1][0], m[1][1], m[1][2],
                            m[2][0], m[2][1], m[2][2] );
    };

    lua.new_usertype<glm::mat3>(
        "mat3",
        sol::call_constructor,
        sol::constructors<glm::mat3()>(),
        sol::meta_function::multiplication,
        mat3_multiply_overloads,
        sol::meta_function::addition,
        mat3_addition_overloads,
        sol::meta_function::subtraction,
        mat3_subtraction_overloads,
        sol::meta_function::index,
        []( glm::mat3& m, int i ) -> glm::vec3& { return m[i]; },
        sol::meta_function::to_string,
        mat3_to_string
    );
}


void CreateMat4Bindings( sol::state& lua )
{
    auto mat4_multiply_overloads =
        sol::overload(
            []( const glm::mat4& m1, const glm::mat4& m2 ) { return m1 * m2; },
            []( const glm::mat4& m, const glm::vec4& v ) { return m * v; },
            []( const glm::mat4& m, float value ) { return m * value; },
            []( float value, const glm::mat4& m ) { return m * value; }
        );

    auto mat4_addition_overloads = []( const glm::mat4& m1, const glm::mat4& m2 ) { return m1 + m2; };

    auto mat4_subtraction_overloads = []( const glm::mat4& m1, const glm::mat4& m2 ) { return m1 - m2; };

    auto mat4_to_string = []( const glm::mat4& m )
    {
        return std::format( "[{}, {}, {}, {},\n"
                            " {}, {}, {}, {},\n"
                            " {}, {}, {}, {},\n"
                            " {}, {}, {}, {}]",
                            m[0][0], m[0][1], m[0][2], m[0][3],
                            m[1][0], m[1][1], m[1][2], m[1][3],
                            m[2][0], m[2][1], m[2][2], m[2][3],
                            m[3][0], m[3][1], m[3][2], m[3][3] );
    };

    lua.new_usertype<glm::mat4>(
        "mat4",
        sol::call_constructor,
        sol::constructors<glm::mat4()>(),
        sol::meta_function::multiplication,
        mat4_multiply_overloads,
        sol::meta_function::addition,
        mat4_addition_overloads,
        sol::meta_function::subtraction,
        mat4_subtraction_overloads,
        sol::meta_function::index,
        []( glm::mat4& m, int i ) -> glm::vec4& { return m[i]; },
        sol::meta_function::to_string,
        mat4_to_string
    );
}

} // namespace bubble
