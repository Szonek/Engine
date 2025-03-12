set(APP_NAME "unit_tests")

set(APP_SOURCES
	main.cpp
	
	basic_tests.cpp
)
add_executable(${APP_NAME} ${APP_SOURCES})
target_link_libraries(${APP_NAME} GTest::gtest_main engine engine_app_toolkit glm)

add_test(NAME ${APP_NAME} COMMAND ${APP_NAME})