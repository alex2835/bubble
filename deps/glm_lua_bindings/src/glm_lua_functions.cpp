#include <sol/sol.hpp>
#include <format>
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include "glm_lua_bindings.hpp"

namespace bubble
{
void CreateMathFreeFunctionsBindings( sol::state& lua )
{
    lua.set_function( "identity_mat2", glm::identity<glm::mat2> );
	lua.set_function( "identity_mat3", glm::identity<glm::mat3> );
	lua.set_function( "identity_mat4", glm::identity<glm::mat4> );

	lua.set_function( "distance",
		sol::overload( 
			[]( glm::vec2& a, glm::vec2& b ) { return glm::distance( a, b ); },
			[]( glm::vec3& a, glm::vec3& b ) { return glm::distance( a, b ); },
			[]( glm::vec4& a, glm::vec4& b ) { return glm::distance( a, b ); }
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
			[]( glm::vec2& a, glm::vec2& b ) { return glm::distance( a, b ); },
			[]( glm::vec3& a, glm::vec3& b ) { return glm::distance( a, b ); },
			[]( glm::vec4& a, glm::vec4& b ) { return glm::distance( a, b ); }
		)
	);

	lua.set_function( "nearly_zero", 
		sol::overload(
			[]( const glm::vec2& v )
			{
				return glm::epsilonEqual( v.x, 0.f, 0.001f ) and
					   glm::epsilonEqual( v.y, 0.f, 0.001f );
			},
			[]( const glm::vec3& v )
			{
				return glm::epsilonEqual( v.x, 0.f, 0.001f ) and
					   glm::epsilonEqual( v.y, 0.f, 0.001f ) and
					   glm::epsilonEqual( v.z, 0.f, 0.001f );
			},
			[]( const glm::vec4& v )
			{
				return glm::epsilonEqual( v.x, 0.f, 0.001f ) and
					   glm::epsilonEqual( v.y, 0.f, 0.001f ) and
					   glm::epsilonEqual( v.z, 0.f, 0.001f ) and
					   glm::epsilonEqual( v.w, 0.f, 0.001f );
			}
		)
	);
}

} // namespace bubble
