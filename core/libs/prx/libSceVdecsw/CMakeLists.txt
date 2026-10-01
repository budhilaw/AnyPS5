add_library(libSceVdecsw SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Export.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AvcParser.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/BlankImage.cpp
)

if(APPLE)
    target_sources(libSceVdecsw PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/VideoToolboxDecoder.cpp)
    target_link_libraries(libSceVdecsw PRIVATE "-framework VideoToolbox" "-framework CoreMedia" "-framework CoreVideo" "-framework CoreFoundation")
elseif(WIN32)
    target_sources(libSceVdecsw PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/MediaFoundationDecoder.cpp)
    target_link_libraries(libSceVdecsw PRIVATE mfuuid wmcodecdspuuid ole32)
    target_link_options(libSceVdecsw PRIVATE "LINKER:--exclude-libs,libmfuuid.a:libwmcodecdspuuid.a")
else()
    target_sources(libSceVdecsw PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/BlankDecoder.cpp)
endif()

target_include_directories(libSceVdecsw PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(libSceVdecsw PRIVATE libc libkernel)
configure_windows_unwind(libSceVdecsw)

if(BUILD_TESTING)
    foreach(runtimeLinkFlag IN ITEMS -static-libgcc -static-libstdc++ -static)
        string(REPLACE "${runtimeLinkFlag}" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    endforeach()
    add_executable(vdecsw_decoding_tests
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/Decoding.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/AvcParser.cpp
    )
    target_include_directories(vdecsw_decoding_tests PRIVATE ${LIBS_INCLUDE_DIR})
    if(WIN32)
        target_sources(vdecsw_decoding_tests PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src/BlankImage.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/MediaFoundationDecoder.cpp
        )
        target_link_libraries(vdecsw_decoding_tests PRIVATE libc mfuuid wmcodecdspuuid ole32)
    endif()
    configure_windows_unwind(vdecsw_decoding_tests)
    add_test(NAME vdecsw_decoding COMMAND vdecsw_decoding_tests)
    if(WIN32)
        set_tests_properties(vdecsw_decoding PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
