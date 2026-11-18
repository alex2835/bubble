# Writes OUTPUT: a C++ file holding INPUT's bytes, behind a function NAME()
# in namespace bubble that gives them as span<const u8> - a file built into a
# program, which then reads none: the editor's font. Run by the build
# whenever INPUT changes; LABEL is how the file names its source.
file( READ "${INPUT}" hex HEX )
string( LENGTH "${hex}" digits )
math( EXPR size "${digits} / 2" )
# Sixteen bytes a line, then each byte as 0x.., - two hex digits never
# straddle a line, as every line has an even number of them.
set( line "" )
foreach( i RANGE 1 32 )
    string( APPEND line "[0-9a-f]" )
endforeach()
string( REGEX REPLACE "(${line})" "\\1\n" hex "${hex}" )
string( REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}" )

set( source "// Made by cmake/embed_file.cmake from ${LABEL}: edit that.
#include \"bubble/types/containers.hpp\"
#include \"bubble/types/number.hpp\"

namespace bubble
{
namespace
{
constexpr u8 cBytes[${size}] = {
${bytes}
};
}

span<const u8> ${NAME}()
{
    return cBytes;
}
}
" )
file( WRITE "${OUTPUT}" "${source}" )
