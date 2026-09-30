add_library(libSceVdecsw SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Export.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AvcParser.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/BlankImage.cpp
)

if(APPLE)
    target_sources(libSceVdecsw PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/VideoToolboxDecoder.cpp)
    target_link_libraries(libSceVdecsw PRIVATE "-framework VideoToolbox" "-framework CoreMedia" "-framework CoreVideo" "-framework CoreFoundation")
else()
    target_sources(libSceVdecsw PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/BlankDecoder.cpp)
endif()

target_include_directories(libSceVdecsw PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(libSceVdecsw PRIVATE libc libkernel)
configure_windows_unwind(libSceVdecsw)
