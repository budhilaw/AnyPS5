add_executable(nid_patcher
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/Sha1.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/NidCompute.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/NidResolver.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/ExportExclusions.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/ElfNidPatcher.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/PeNidPatcher.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/BinaryPatcherFactory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/main.cpp
)
target_include_directories(nid_patcher PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/nid/include)
target_compile_features(nid_patcher PRIVATE cxx_std_20)

if(MINGW)
    add_compile_options(-fno-asynchronous-unwind-tables)
endif()

function(configure_windows_unwind target)
    if(MINGW)
        if(NOT EXISTS "${CMAKE_OBJCOPY}")
            message(FATAL_ERROR "Windows DWARF unwinding requires objcopy")
        endif()
        add_custom_command(TARGET ${target} POST_BUILD
                COMMAND "${CMAKE_OBJCOPY}" --rename-section .eh_frame=.ehfram "$<TARGET_FILE:${target}>"
                VERBATIM
        )
    endif()
endfunction()

if(APPLE AND ANYPS5_HOST_X86_64)
    add_link_options(-Wl,-no_compact_unwind)
endif()

if(APPLE)
    set(CMAKE_CXX_CREATE_SHARED_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/nid/darwin/link-with-nid-aliases.sh ${CMAKE_CURRENT_BINARY_DIR}/nid_patcher ${CMAKE_CXX_CREATE_SHARED_LIBRARY}")
    set(CMAKE_C_CREATE_SHARED_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/nid/darwin/link-with-nid-aliases.sh ${CMAKE_CURRENT_BINARY_DIR}/nid_patcher ${CMAKE_C_CREATE_SHARED_LIBRARY}")
endif()

set(LIBC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/prx/libc)
add_library(libc SHARED EXCLUDE_FROM_ALL)
set(LIBS_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory(${LIBC_DIR} ${CMAKE_CURRENT_BINARY_DIR}/prx/libc)
target_include_directories(libc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
configure_windows_unwind(libc)
if(APPLE)
    add_dependencies(libc nid_patcher)
endif()

set(UNPATCHED_DIR ${CMAKE_CURRENT_BINARY_DIR}/libs/unpatched)
set(PATCHED_DIR ${CMAKE_CURRENT_BINARY_DIR}/libs)

set_target_properties(libc PROPERTIES
        PREFIX ""
        SUFFIX .prx
        RUNTIME_OUTPUT_DIRECTORY ${UNPATCHED_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${UNPATCHED_DIR}
)

set(LIBC_UNPATCHED ${UNPATCHED_DIR}/libc.prx)
set(LIBC_PATCHED ${PATCHED_DIR}/libc.prx)

if(APPLE)
    set(NID_PATCH_LIBC_COMMAND ${CMAKE_COMMAND} -E echo "NID patching skipped for libc: Mach-O export patching is not implemented")
    set(NID_PATCH_COMMENT_PREFIX "Copying (unpatched) ")
else()
    set(NID_PATCH_LIBC_COMMAND ${CMAKE_CURRENT_BINARY_DIR}/nid_patcher libc ${LIBC_PATCHED})
    set(NID_PATCH_COMMENT_PREFIX "Patching NIDs for ")
endif()

add_custom_command(
        OUTPUT ${LIBC_PATCHED}
        COMMAND ${CMAKE_COMMAND} -E copy ${LIBC_UNPATCHED} ${LIBC_PATCHED}
        COMMAND ${NID_PATCH_LIBC_COMMAND}
        DEPENDS nid_patcher libc ${LIBC_UNPATCHED}
        COMMENT "${NID_PATCH_COMMENT_PREFIX}libc"
)

file(GLOB LIBS_DIRS LIST_DIRECTORIES true ${CMAKE_CURRENT_SOURCE_DIR}/prx/*)
list(REMOVE_ITEM LIBS_DIRS ${LIBC_DIR})

set(PATCHED_OUTPUTS ${LIBC_PATCHED})

foreach(LIB_DIR ${LIBS_DIRS})
    if(IS_DIRECTORY ${LIB_DIR})
        get_filename_component(LIB_NAME ${LIB_DIR} NAME)

        if(EXISTS ${LIB_DIR}/CMakeLists.txt)
            set(LIBS_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
            add_subdirectory(${LIB_DIR} ${CMAKE_CURRENT_BINARY_DIR}/prx/${LIB_NAME})
        else()
            file(GLOB LIB_SOURCES CONFIGURE_DEPENDS ${LIB_DIR}/*.cpp)
            add_library(${LIB_NAME} SHARED EXCLUDE_FROM_ALL ${LIB_SOURCES})
            target_include_directories(${LIB_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
            configure_windows_unwind(${LIB_NAME})
        endif()

        if(APPLE)
            add_dependencies(${LIB_NAME} nid_patcher)
        endif()
        if(LIB_NAME STREQUAL "libkernel")
            target_link_libraries(${LIB_NAME} PRIVATE libc)
        else()
            target_link_libraries(${LIB_NAME} PRIVATE libkernel libc)
        endif()
        if(LIB_NAME MATCHES "^libSceAjm")
            target_link_libraries(${LIB_NAME} PRIVATE atrac9)
        endif()

        set_target_properties(${LIB_NAME} PROPERTIES
                PREFIX ""
                SUFFIX .prx
                RUNTIME_OUTPUT_DIRECTORY ${UNPATCHED_DIR}
                LIBRARY_OUTPUT_DIRECTORY ${UNPATCHED_DIR}
        )

        set(UNPATCHED_FILE ${UNPATCHED_DIR}/${LIB_NAME}.prx)
        set(PATCHED_FILE ${PATCHED_DIR}/${LIB_NAME}.prx)

        if(APPLE)
            set(NID_PATCH_COMMAND ${CMAKE_COMMAND} -E echo "NID patching skipped for ${LIB_NAME}: Mach-O export patching is not implemented")
        else()
            set(NID_PATCH_COMMAND ${CMAKE_CURRENT_BINARY_DIR}/nid_patcher ${LIB_NAME} --preserve-exports "${LIBC_UNPATCHED}" "${PATCHED_FILE}")
        endif()

        add_custom_command(
                OUTPUT ${PATCHED_FILE}
                COMMAND ${CMAKE_COMMAND} -E copy ${UNPATCHED_FILE} ${PATCHED_FILE}
                COMMAND ${NID_PATCH_COMMAND}
                DEPENDS nid_patcher ${LIB_NAME} ${UNPATCHED_FILE} ${LIBC_UNPATCHED}
                COMMENT "${NID_PATCH_COMMENT_PREFIX}${LIB_NAME}"
        )

        list(APPEND PATCHED_OUTPUTS ${PATCHED_FILE})
    endif()
endforeach()

add_custom_target(libs DEPENDS ${PATCHED_OUTPUTS})

if(BUILD_TESTING)
    add_executable(guest_rendering_wait_tests tests/GuestRenderingWait.cpp)
    target_include_directories(guest_rendering_wait_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_rendering_wait_tests PRIVATE libSceAgcDriver libc)
    configure_windows_unwind(guest_rendering_wait_tests)
    add_test(NAME guest_rendering_wait COMMAND guest_rendering_wait_tests)
    if(WIN32)
        set_tests_properties(guest_rendering_wait PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_executable(guest_buffer_reuse_tests tests/GuestBufferReuse.cpp)
    target_include_directories(guest_buffer_reuse_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_buffer_reuse_tests PRIVATE libc)
    configure_windows_unwind(guest_buffer_reuse_tests)
    add_test(NAME guest_buffer_reuse COMMAND guest_buffer_reuse_tests)
    if(WIN32)
        set_tests_properties(guest_buffer_reuse PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_executable(guest_shader_link_tests tests/GuestShaderLink.cpp)
    target_include_directories(guest_shader_link_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_shader_link_tests PRIVATE libSceAgc libc)
    configure_windows_unwind(guest_shader_link_tests)
    add_test(NAME guest_shader_link COMMAND guest_shader_link_tests)
    if(WIN32)
        set_tests_properties(guest_shader_link PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_executable(guest_system_exit_tests tests/GuestSystemExit.cpp)
    target_include_directories(guest_system_exit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_system_exit_tests PRIVATE libSceSystemService libc)
    configure_windows_unwind(guest_system_exit_tests)
    add_test(NAME guest_system_exit_parameters COMMAND guest_system_exit_tests)
    add_test(NAME guest_system_exit COMMAND guest_system_exit_tests --exit)
    add_test(NAME guest_immediate_exit COMMAND guest_system_exit_tests --immediate)
    set_tests_properties(guest_system_exit PROPERTIES
        PASS_REGULAR_EXPRESSION "Guest shutdown and atexit completed")
    if(WIN32)
        set_tests_properties(guest_system_exit guest_system_exit_parameters guest_immediate_exit PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_executable(guest_character_tests tests/GuestCharacter.cpp)
    target_include_directories(guest_character_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_character_tests PRIVATE libc)
    configure_windows_unwind(guest_character_tests)
    add_test(NAME guest_character COMMAND guest_character_tests)
    if(WIN32)
        set_tests_properties(guest_character PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_executable(guest_math_tests tests/GuestMath.cpp)
    target_include_directories(guest_math_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_math_tests PRIVATE libc)
    configure_windows_unwind(guest_math_tests)
    add_test(NAME guest_math COMMAND guest_math_tests)
    if(WIN32)
        set_tests_properties(guest_math PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_executable(guest_working_directory_tests tests/GuestWorkingDirectory.cpp)
    target_include_directories(guest_working_directory_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_working_directory_tests PRIVATE libc)
    configure_windows_unwind(guest_working_directory_tests)
    add_test(NAME guest_working_directory COMMAND guest_working_directory_tests)
    if(WIN32)
        set_tests_properties(guest_working_directory PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_library(guest_module_fixture SHARED tests/GuestModuleFixture.cpp)
    target_include_directories(guest_module_fixture PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    if(APPLE)
        add_dependencies(guest_module_fixture nid_patcher)
    else()
        add_custom_command(TARGET guest_module_fixture POST_BUILD
            COMMAND ${CMAKE_CURRENT_BINARY_DIR}/nid_patcher guest_module_fixture $<TARGET_FILE:guest_module_fixture>
            VERBATIM)
        add_dependencies(guest_module_fixture nid_patcher)
    endif()
    add_executable(guest_dynamic_loader_tests tests/GuestDynamicLoader.cpp)
    target_include_directories(guest_dynamic_loader_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_dynamic_loader_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_dynamic_loader_tests)
    add_dependencies(guest_dynamic_loader_tests guest_module_fixture)
    add_test(NAME guest_dynamic_loader COMMAND guest_dynamic_loader_tests $<TARGET_FILE_NAME:guest_module_fixture>)
    set_tests_properties(guest_dynamic_loader PROPERTIES
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:guest_module_fixture>")
    if(WIN32)
        set_tests_properties(guest_dynamic_loader PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    add_executable(guest_filesystem_tests tests/GuestFilesystem.cpp)
    target_include_directories(guest_filesystem_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_filesystem_tests PRIVATE libc)
    configure_windows_unwind(guest_filesystem_tests)
    add_test(NAME guest_filesystem COMMAND guest_filesystem_tests)
    if(WIN32)
        set_tests_properties(guest_filesystem PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_executable(guest_directory_tests tests/GuestDirectory.cpp)
    target_include_directories(guest_directory_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_directory_tests PRIVATE libc)
    configure_windows_unwind(guest_directory_tests)
    add_test(NAME guest_directory COMMAND guest_directory_tests)
    if(WIN32)
        set_tests_properties(guest_directory PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_executable(guest_environment_tests tests/GuestEnvironment.cpp)
    target_include_directories(guest_environment_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_environment_tests PRIVATE libc)
    configure_windows_unwind(guest_environment_tests)
    add_test(NAME guest_environment COMMAND guest_environment_tests)
    if(WIN32)
        set_tests_properties(guest_environment PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    if(ANYPS5_GUEST_ABI)
        add_executable(guest_standard_stream_tests tests/GuestStandardStreams.cpp)
        target_include_directories(guest_standard_stream_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        target_link_libraries(guest_standard_stream_tests PRIVATE libc)
        configure_windows_unwind(guest_standard_stream_tests)
        add_test(NAME guest_standard_streams COMMAND guest_standard_stream_tests)
        if(WIN32)
            set_tests_properties(guest_standard_streams PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endif()
    add_executable(guest_once_tests tests/GuestOnce.cpp)
    target_include_directories(guest_once_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_once_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_once_tests)
    add_test(NAME guest_once COMMAND guest_once_tests)
    set_tests_properties(guest_once PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_once PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    add_executable(guest_signal_tests tests/GuestSignals.cpp)
    target_include_directories(guest_signal_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_signal_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_signal_tests)
    add_test(NAME guest_signals COMMAND guest_signal_tests)
    if(WIN32)
        set_tests_properties(guest_signals PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    add_executable(guest_system_configuration_tests tests/GuestSystemConfiguration.cpp)
    target_include_directories(guest_system_configuration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_system_configuration_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_system_configuration_tests)
    add_test(NAME guest_system_configuration COMMAND guest_system_configuration_tests)
    if(WIN32)
        set_tests_properties(guest_system_configuration PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    add_executable(guest_udp_tests tests/GuestUdp.cpp)
    target_include_directories(guest_udp_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_udp_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_udp_tests)
    add_test(NAME guest_udp COMMAND guest_udp_tests)
    set_tests_properties(guest_udp PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_udp PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    add_executable(guest_address_tests tests/GuestAddressConversion.cpp)
    target_include_directories(guest_address_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_address_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_address_tests)
    add_test(NAME guest_addresses COMMAND guest_address_tests)
    if(WIN32)
        set_tests_properties(guest_addresses PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    add_executable(guest_error_tests tests/GuestErrors.cpp)
    target_include_directories(guest_error_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_error_tests PRIVATE libc)
    configure_windows_unwind(guest_error_tests)
    add_test(NAME guest_errors COMMAND guest_error_tests)
    if(WIN32)
        set_tests_properties(guest_errors PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_executable(guest_resolver_tests tests/GuestResolver.cpp)
    target_include_directories(guest_resolver_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_resolver_tests PRIVATE libScePosixForWebKit)
    add_test(NAME guest_resolver COMMAND guest_resolver_tests)
    if(WIN32)
        set_tests_properties(guest_resolver PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libScePosixForWebKit>")
    endif()
    add_executable(mspace_tests tests/Mspace.cpp)
    target_include_directories(mspace_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(mspace_tests PRIVATE libc)
    configure_windows_unwind(mspace_tests)
    add_test(NAME mspace COMMAND mspace_tests)
    if(WIN32)
        set_tests_properties(mspace PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_executable(guest_memory_tests tests/GuestMemory.cpp)
    target_include_directories(guest_memory_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_memory_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_memory_tests)
    add_test(NAME guest_memory COMMAND guest_memory_tests)
    if(WIN32)
        set_tests_properties(guest_memory PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    add_executable(guest_search_tests tests/GuestSearch.cpp)
    target_include_directories(guest_search_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_search_tests PRIVATE libc)
    configure_windows_unwind(guest_search_tests)
    add_test(NAME guest_search COMMAND guest_search_tests)
    if(WIN32)
        set_tests_properties(guest_search PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_executable(guest_string_tests tests/GuestStrings.cpp)
    target_include_directories(guest_string_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_string_tests PRIVATE libc libScePosixForWebKit)
    configure_windows_unwind(guest_string_tests)
    add_test(NAME guest_strings COMMAND guest_string_tests)
    if(WIN32)
        set_tests_properties(guest_strings PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_executable(application_heap_tests EXCLUDE_FROM_ALL tests/ApplicationHeap.cpp prx/libc/src/ApplicationHeap.cpp prx/libc/src/ApplicationProcessParameters.cpp)
if(APPLE)
    target_sources(application_heap_tests PRIVATE prx/libc/src/specifics/darwin/GuestImage.cpp)
endif()
target_include_directories(application_heap_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(application_heap_tests PROPERTIES CXX_EXTENSIONS OFF)

add_executable(guest_locale_tests EXCLUDE_FROM_ALL tests/GuestLocale.cpp)
target_include_directories(guest_locale_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(guest_locale_tests PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(guest_locale_tests PRIVATE libc)
configure_windows_unwind(guest_locale_tests)

if(MINGW)
    foreach(runtimeLinkFlag IN ITEMS -static-libgcc -static-libstdc++ -static)
        string(REPLACE "${runtimeLinkFlag}" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    endforeach()
    add_executable(windows_exception_tests EXCLUDE_FROM_ALL tests/WindowsExceptions.cpp)
    set_target_properties(windows_exception_tests PROPERTIES CXX_EXTENSIONS OFF)
    target_link_libraries(windows_exception_tests PRIVATE libc)
    configure_windows_unwind(windows_exception_tests)
endif()
