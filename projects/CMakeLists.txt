add_subdirectory( engine )
add_subdirectory( import )
add_subdirectory( editing )
add_subdirectory( runner )
add_subdirectory( play )

if( BUBBLE_BUILD_TESTS )
    add_subdirectory( tests )
endif()
