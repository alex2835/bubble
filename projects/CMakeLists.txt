add_subdirectory( engine )
add_subdirectory( import )
add_subdirectory( editing )

if( BUBBLE_BUILD_TESTS )
    add_subdirectory( tests )
endif()
