
#include <sol/forward.hpp>

namespace bubble
{
void CreateVec2Bindings( sol::state& lua );
void CreateVec3Bindings( sol::state& lua );
void CreateVec4Bindings( sol::state& lua );

void CreateMat2Bindings( sol::state& lua );
void CreateMat3Bindings( sol::state& lua );
void CreateMat4Bindings( sol::state& lua );

void CreateMathFreeFunctionsBindings( sol::state& lua );
}
