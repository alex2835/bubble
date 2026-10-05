// Luau's type checker and native code generation build and work here. The
// editor will check scripts on import with the first; the engine may compile
// hot scripts to machine code with the second (desktop and Android only).
#include <Luau/BuiltinDefinitions.h>
#include <Luau/ConfigResolver.h>
#include <Luau/Error.h>
#include <Luau/FileResolver.h>
#include <Luau/Frontend.h>
#include <Luau/TypeArena.h>
#include <doctest.h>
#include <map>
#include <string>
#include <vector>

#ifdef BUBBLE_LUAU_CODEGEN
#include <cstdlib>
#include <lua.h>
#include <luacode.h>
#include <luacodegen.h>
#include <lualib.h>
#endif

namespace
{
// Scripts from memory, by name.
struct MemoryFiles : Luau::FileResolver
{
    std::map<std::string, std::string> mFiles;

    std::optional<Luau::SourceCode> readSource( const Luau::ModuleName& name ) override
    {
        const auto it = mFiles.find( name );
        if ( it == mFiles.end() )
            return std::nullopt;
        return Luau::SourceCode{ it->second, Luau::SourceCode::Module };
    }
};

std::vector<std::string> Check( const std::string& source )
{
    MemoryFiles files;
    files.mFiles["main"] = source;
    Luau::NullConfigResolver config;
    Luau::Frontend frontend( &files, &config );
    Luau::registerBuiltinGlobals( frontend, frontend.globals );
    Luau::freeze( frontend.globals.globalTypes );

    std::vector<std::string> errors;
    for ( const Luau::TypeError& error : frontend.check( "main" ).errors )
        errors.push_back( Luau::toString( error ) );
    return errors;
}
}

TEST_CASE( "The type checker passes a correct script" )
{
    CHECK( Check( R"(--!strict
        local function length(v: vector): number
            return vector.magnitude(v)
        end
        local l: number = length(vector.create(3, 4, 0))
    )" )
               .empty() );
}

TEST_CASE( "The type checker finds a mistake before the script runs" )
{
    const auto errors = Check( R"(--!strict
        local speed: number = "fast"
    )" );
    REQUIRE( errors.size() == 1 );
    CHECK( errors[0].find( "string" ) != std::string::npos );
}

#ifdef BUBBLE_LUAU_CODEGEN
TEST_CASE( "Native code gives the same result as the interpreter" )
{
    if ( not luau_codegen_supported() )
        return;

    const std::string source = R"(
        local sum = vector.zero
        for i = 1, 1000 do
            sum += vector.create(i, i * 2, 1) * 0.5
        end
        return sum.x + sum.y + sum.z
    )";
    const auto run = [&]( bool native ) {
        lua_State* L = luaL_newstate();
        luaL_openlibs( L );
        if ( native )
            luau_codegen_create( L );
        size_t size = 0;
        char* bytecode = luau_compile( source.data(), source.size(), nullptr, &size );
        REQUIRE( luau_load( L, "=native", bytecode, size, 0 ) == 0 );
        std::free( bytecode );
        if ( native )
            luau_codegen_compile( L, -1 );
        REQUIRE( lua_pcall( L, 0, 1, 0 ) == 0 );
        const double result = lua_tonumber( L, -1 );
        lua_close( L );
        return result;
    };
    CHECK( run( true ) == run( false ) );
    CHECK( run( false ) == doctest::Approx( 0.5 * ( 500500 + 1001000 + 1000 ) ) );
}
#endif
