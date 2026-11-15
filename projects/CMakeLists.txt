add_subdirectory( engine )
add_subdirectory( import )
add_subdirectory( editing )
add_subdirectory( runner )

if( BUBBLE_BUILD_TESTS )
    add_subdirectory( tests )
endif()
