add_library(libSceMsgDialog.native SHARED EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../libSceMsgDialog/Export.cpp)
target_include_directories(libSceMsgDialog.native PRIVATE ${LIBS_INCLUDE_DIR})
