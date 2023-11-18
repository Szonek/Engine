set(APP_NAME "project_c_common")

add_library(${APP_NAME} INTERFACE)
target_include_directories(${APP_NAME} INTERFACE ./)
add_custom_target(${APP_NAME}_ SOURCES network_message_types.h)