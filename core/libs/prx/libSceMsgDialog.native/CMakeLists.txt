add_library(libSceMsgDialog.native SHARED EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp)
target_include_directories(libSceMsgDialog.native PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(libSceMsgDialog.native PRIVATE libSceMsgDialog)
