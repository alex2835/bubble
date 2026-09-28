#include "test.hpp"
#include "engine/utils/lookalike_text.hpp"

TEST( LookalikeText )
{
    // "camera" with a Cyrillic es and a Cyrillic a in it.
    const string mixed = "\xD1\x81" "\xD0\xB0" "mera";
    CHECK( LooksAlike( mixed, "camera" ) );
    CHECK( LooksAlike( "Camera", "camera" ) );
    CHECK( not LooksAlike( "camera", "cameras" ) );
    // A Cyrillic letter that is not drawn like a Latin one stays itself.
    CHECK( not LooksAlike( "\xD0\xB4" "oor", "door" ) );

    CHECK( NonAsciiLetters( "camera" ).empty() );
    CHECK( NonAsciiLetters( mixed ) == "'\xD1\x81' (U+0441), '\xD0\xB0' (U+0430)" );
}
