project(schedule_web)

find_package(Poco COMPONENTS Net Util REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

file(GLOB SRC_FILES "src/*.cpp")
add_executable(${PROJECT_NAME} ${SRC_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC include ../schedule_gen/include)
target_link_libraries(${PROJECT_NAME} PUBLIC lib_schedule_gen Poco::Net Poco::Util nlohmann_json::nlohmann_json spdlog::spdlog)

add_library(lib_${PROJECT_NAME} STATIC ${SRC_FILES})
target_include_directories(lib_${PROJECT_NAME} PUBLIC include ../schedule_gen/include)
target_link_libraries(lib_${PROJECT_NAME} PUBLIC lib_schedule_gen Poco::Net Poco::Util nlohmann_json::nlohmann_json spdlog::spdlog)

file(GLOB TEST_FILES "tests/*.cpp")
add_executable(Catch_test_${PROJECT_NAME} ${TEST_FILES})
target_link_libraries(Catch_test_${PROJECT_NAME} PUBLIC catch_main lib_${PROJECT_NAME})

catch_discover_tests(
    Catch_test_${PROJECT_NAME}
    TEST_PREFIX
    "unittests."
    REPORTER
    xml
    OUTPUT_DIR
    .
    OUTPUT_PREFIX
    "unittests."
    OUTPUT_SUFFIX
    .xml)
