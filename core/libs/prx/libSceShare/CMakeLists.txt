add_library(libSceShare SHARED EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/Shim.cpp)
target_include_directories(libSceShare PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(libSceShare PRIVATE libSceShare.native)
configure_windows_unwind(libSceShare)
