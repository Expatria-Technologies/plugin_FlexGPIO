add_library(flexgpio INTERFACE)

target_sources(flexgpio INTERFACE
 ${CMAKE_CURRENT_LIST_DIR}/flexgpio.c
)

target_include_directories(flexgpio INTERFACE ${CMAKE_CURRENT_LIST_DIR})
