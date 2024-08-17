
#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>
#include "engine/utils/types.hpp"
#include "engine/scripting/glm_bindings.hpp"

namespace bubble
{
void CreateVec2Bind( sol::state& lua )
{
	auto vec2_multiply_overloads = 
		sol::overload( 
			[]( const vec2& v1, const vec2& v2 ) { return v1 * v2; },
			[]( const vec2& v, float value ) { return v * value; },
			[]( float value, const vec2& v ) { return v * value; }
		);

	auto vec2_divide_overloads = 
		sol::overload( []( const vec2& v1, const vec2& v2 ) { return v1 / v2; },
					   []( const vec2& v, float value ) { return v / value; },
					   []( float value, const vec2& v ) { return v / value; }
		);

	auto vec2_addition_overloads = 
		sol::overload( []( const vec2& v1, const vec2& v2 ) { return v1 + v2; },
					   []( const vec2& v, float value ) { return v + value; },
					   []( float value, const vec2& v1 ) { return v1 + value; }
		);

	auto vec2_subtraction_overloads = 
		sol::overload( []( const vec2& v1, const vec2& v2 ) { return v1 - v2; },
					   []( const vec2& v, float value ) { return v - value; },
					   []( float value, const vec2& v ) { return v - value; }
		);

	lua.new_usertype<vec2>(
		"vec2",
		sol::call_constructor,
		sol::constructors<vec2(), vec2( float ), vec2( float, float )>(),
		"x",
		&vec2::x,
		"y",
		&vec2::y,
		sol::meta_function::multiplication,
		vec2_multiply_overloads,
		sol::meta_function::division,
		vec2_divide_overloads,
		sol::meta_function::addition,
		vec2_addition_overloads,
		sol::meta_function::subtraction,
		vec2_subtraction_overloads,
        sol::meta_function::index,
        []( const vec2& v, int i ) { return v[i]; },
        sol::meta_function::new_index,
        []( vec2& v, int i, float value ) { v[i] = value; },
        sol::meta_function::to_string,
        []( const vec2& v ) { return std::format( "[{}, {}]", v.x, v.y ); },
		"length",
		[]( const vec2& v ) { return length( v ); },
		"lengthSq",
		[]( const vec2& v ) { return length2( v ); },
		"normalize",
		[]( const vec2& v ) { return normalize( v ); },
		"normalize2",
		[]( const vec2& v1, const vec2& v2 ) { return normalize( v2 - v1 ); }
	);
}


void CreateVec3Bind( sol::state& lua )
{
	auto vec3_multiply_overloads = 
		sol::overload( 
			[]( const vec3& v1, const vec3& v2 ) { return v1 * v2; },
			[]( const vec3& v, float value ) { return v * value; },
			[]( float value, const vec3& v ) { return v * value; }
		);

	auto vec3_divide_overloads = 
		sol::overload( 
			[]( const vec3& v1, const vec3& v2 ) { return v1 / v2; },
            []( const vec3& v, float value ) { return v / value; },
            []( float value, const vec3& v ) { return v / value; }
		);

	auto vec3_addition_overloads = 
		sol::overload( 
			[]( const vec3& v1, const vec3& v2 ) { return v1 + v2; },
            []( const vec3& v, float value ) { return v + value; },
            []( float value, const vec3& v ) { return v + value; }
		);

	auto vec3_subtraction_overloads = 
		sol::overload( 
			[]( const vec3& v1, const vec3& v2 ) { return v1 - v2; },
			[]( const vec3& v, float value ) { return v - value; },
			[]( float value, const vec3& v ) { return v - value; }
		);

	lua.new_usertype<vec3>(
		"vec3",
		sol::call_constructor,
		sol::constructors<vec3(), vec3( float ), vec3( float, float, float )>(),
		"x",
		&vec3::x,
		"y",
		&vec3::y,
		"z",
		&vec3::z,
		sol::meta_function::multiplication,
		vec3_multiply_overloads,
		sol::meta_function::division,
		vec3_divide_overloads,
		sol::meta_function::addition,
		vec3_addition_overloads,
		sol::meta_function::subtraction,
		vec3_subtraction_overloads,
        sol::meta_function::index,
        []( const vec3& v, int i ) { return v[i]; },
        sol::meta_function::new_index,
        []( vec3& v, int i, float value ) { v[i] = value; },
		sol::meta_function::to_string,
		[]( const vec3& v ) { return std::format("[{}, {}, {}]", v.x, v.y, v.z ); },
		"length",
		[]( const vec3& v ) { return length( v ); },
		"lengthSq",
		[]( const vec3& v ) { return length2( v ); },
		"normalize",
		[]( const vec3& v1 ) { return normalize( v1 ); },
		"normalize2",
		[]( const vec3& v1, const vec3& v2 ) { return normalize( v2 - v1 ); }
	);
}


void CreateVec4Bind( sol::state& lua )
{
	auto vec4_multiply_overloads = 
		sol::overload( 
			[]( const vec4& v1, const vec4& v2 ) { return v1 * v2; },
			[]( const vec4& v, float value ) { return v * value; },
			[]( float value, const vec4& v ) { return v * value; } 
		);

	auto vec4_divide_overloads = 
		sol::overload( 
			[]( const vec4& v1, const vec4& v2 ) { return v1 / v2; },
			[]( const vec4& v, float value ) { return v / value; },
			[]( float value, const vec4& v ) { return v / value; } 
		);

	auto vec4_addition_overloads = 
		sol::overload( 
			[]( const vec4& v1, const vec4& v2 ) { return v1 + v2; },
			[]( const vec4& v, float value ) { return v + value; },
			[]( float value, const vec4& v ) { return v + value; } 
		);

	auto vec4_subtraction_overloads = 
		sol::overload( 
			[]( const vec4& v1, const vec4& v2 ) { return v1 - v2; },
			[]( const vec4& v1, float value ) { return v1 - value; },
			[]( float value, const vec4& v1 ) { return v1 - value; } 
		);

	lua.new_usertype<vec4>(
		"vec4",
		sol::call_constructor,
		sol::constructors<vec4( float ), vec4( float, float, float, float )>(),
		"x",
		&vec4::x,
		"y",
		&vec4::y,
		"z",
		&vec4::z,
		"w",
		&vec4::w,
		sol::meta_function::multiplication,
		vec4_multiply_overloads,
		sol::meta_function::division,
		vec4_divide_overloads,
		sol::meta_function::addition,
		vec4_addition_overloads,
		sol::meta_function::subtraction,
		vec4_subtraction_overloads,
        sol::meta_function::index,
        []( const vec4& v, int i ) { return v[i]; },
        sol::meta_function::new_index,
        []( vec4& v, int i, float value ) { v[i] = value; },
        sol::meta_function::to_string,
        []( const vec4& v ) { return std::format( "[{}, {}, {}, {}]", v.x, v.y, v.z, v.w ); },
		"length",
		[]( const vec4& v ) { return length( v ); },
		"lengthSq",
		[]( const vec4& v ) { return length2( v ); },
		"normalize",
		[]( const vec4& v1 ) { return normalize( v1 ); },
		"normalize2",
		[]( const vec4& v1, const vec4& v2 ) { return normalize( v2 - v1 ); }
	);
}

// mat2
void CreateMat2Bind( sol::state& lua )
{
    auto mat2_multiply_overloads = 
		sol::overload( 
			[]( const mat2& m1, const mat2& m2 ) { return m1 * m2; },
			[]( const mat2& m, const vec2& v ) { return m * v; },
			[]( const mat2& m, float value ) { return m * value; },
			[]( float value, const mat2& m ) { return m * value; } 
		);

    auto mat2_addition_overloads = []( const mat2& m1, const mat2& m2 ) { return m1 + m2; };

	auto mat2_subtraction_overloads = []( const mat2& m1, const mat2& m2 ) { return m1 - m2; };

	auto mat2_to_string = []( const mat2& m )
	{
        return std::format( "[{}, {},\n"
                            " {}, {}]",
                            m[0][0], m[0][1],
                            m[1][0], m[1][1] );
	};

	lua.new_usertype<mat2>(
		"mat2",
		sol::call_constructor,
		sol::constructors<mat2()>(),
		sol::meta_function::multiplication,
		mat2_multiply_overloads,
		sol::meta_function::addition,
		mat2_addition_overloads,
		sol::meta_function::subtraction,
		mat2_subtraction_overloads,
		sol::meta_function::index,
		[]( mat2& m, int i ) -> vec2& { return m[i]; },
		sol::meta_function::to_string,
		mat2_to_string
	);
}

// mat3
void CreateMat3Bind( sol::state& lua )
{
    auto mat3_multiply_overloads = 
		sol::overload( 
			[]( const mat3& m1, const mat3& m2 ) { return m1 * m2; },
			[]( const mat3& m, const vec3& v ) { return m * v; },
			[]( const mat3& m, float value ) { return m * value; },
			[]( float value, const mat3& m ) { return m * value; } 
		);

    auto mat3_addition_overloads = []( const mat3& m1, const mat3& m2 ) { return m1 + m2; };

    auto mat3_subtraction_overloads = []( const mat3& m1, const mat3& m2 ) { return m1 - m2; };

	auto mat3_to_string = []( const mat3& m )
	{
		return std::format( "[{}, {}, {},\n"
							" {}, {}, {},\n"
							" {}, {}, {}]",
							m[0][0], m[0][1], m[0][2],
							m[1][0], m[1][1], m[1][2],
							m[2][0], m[2][1], m[2][2] );
	};

    lua.new_usertype<mat3>(
        "mat3",
        sol::call_constructor,
        sol::constructors<mat3()>(),
        sol::meta_function::multiplication,
        mat3_multiply_overloads,
        sol::meta_function::addition,
        mat3_addition_overloads,
        sol::meta_function::subtraction,
        mat3_subtraction_overloads,
        sol::meta_function::index,
        []( mat3& m, int i ) -> vec3& { return m[i]; },
        sol::meta_function::to_string,
		mat3_to_string
    );
}

// mat4
void CreateMat4Bind( sol::state& lua )
{
    auto mat4_multiply_overloads = 
		sol::overload( 
			[]( const mat4& m1, const mat4& m2 ) { return m1 * m2; },
			[]( const mat4& m, const vec4& v ) { return m * v; },
			[]( const mat4& m, float value ) { return m * value; },
			[]( float value, const mat4& m ) { return m * value; } 
		);

    auto mat4_addition_overloads = []( const mat4& m1, const mat4& m2 ) { return m1 + m2; };

    auto mat4_subtraction_overloads = []( const mat4& m1, const mat4& m2 ) { return m1 - m2; };

    auto mat4_to_string = []( const mat4& m )
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

	lua.new_usertype<mat4>(
		"mat4",
		sol::call_constructor,
		sol::constructors<mat4()>(),
		sol::meta_function::multiplication,
		mat4_multiply_overloads,
		sol::meta_function::addition,
		mat4_addition_overloads,
		sol::meta_function::subtraction,
		mat4_subtraction_overloads,
        sol::meta_function::index,
        []( mat4& m, int i ) -> vec4& { return m[i]; },
		sol::meta_function::to_string,
		mat4_to_string
    );
}


void MathFreeFunctions( sol::state& lua )
{
    lua.set_function( "identity_mat2", identity<mat2> );
	lua.set_function( "identity_mat3", identity<mat3> );
	lua.set_function( "identity_mat4", identity<mat4> );

	lua.set_function( "distance",
		sol::overload( 
			[]( vec2& a, vec2& b ) { return distance( a, b ); },
			[]( vec3& a, vec3& b ) { return distance( a, b ); },
			[]( vec4& a, vec4& b ) { return distance( a, b ); }
		) 
	);

	lua.set_function( "lerp", []( float a, float b, float f ) { return std::lerp( a, b, f ); } );
	
	lua.set_function( "clamp",
		sol::overload( 
			[]( float value, float min, float max ) { return std::clamp( value, min, max ); },
			[]( double value, double min, double max ) { return std::clamp( value, min, max ); },
			[]( int value, int min, int max ) { return std::clamp( value, min, max ); }
		)
	);

	lua.set_function( "distance",
		sol::overload( 
			[]( vec2& a, vec2& b ) { return distance( a, b ); },
			[]( vec3& a, vec3& b ) { return distance( a, b ); },
			[]( vec4& a, vec4& b ) { return distance( a, b ); }
		)
	);

	lua.set_function( "nearly_zero", 
		sol::overload(
			[]( const vec2& v )
			{
				return epsilonEqual( v.x, 0.f, 0.001f ) and
					   epsilonEqual( v.y, 0.f, 0.001f );
			},
			[]( const vec3& v )
			{
				return epsilonEqual( v.x, 0.f, 0.001f ) and
					   epsilonEqual( v.y, 0.f, 0.001f ) and
					   epsilonEqual( v.z, 0.f, 0.001f );
			},
			[]( const vec4& v )
			{
				return epsilonEqual( v.x, 0.f, 0.001f ) and
					   epsilonEqual( v.y, 0.f, 0.001f ) and
					   epsilonEqual( v.z, 0.f, 0.001f ) and
					   epsilonEqual( v.w, 0.f, 0.001f );
			}
		)
	);
}


void CreateGLMBindings( sol::state& lua )
{
	CreateVec2Bind( lua );
	CreateVec3Bind( lua );
	CreateVec4Bind( lua );
	CreateMat2Bind( lua );
	CreateMat3Bind( lua );
	CreateMat4Bind( lua );
	MathFreeFunctions( lua );
}

} 
