# Overall WarpEngine requires CMake 3.28+
# This is because some features from Vulkan SDK require CMake 3.28+
cmake_minimum_required(VERSION 3.28)

# Setting up GTest here
# see reference https://google.github.io/googletest/quickstart-cmake.html

# GoogleTest requires at least C++17
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest
    GIT_TAG 52eb810
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

function(add_warp_test test_folder_name)
    include("${CMAKE_CURRENT_SOURCE_DIR}/${test_folder_name}/CMakeLists.txt")
    include(GoogleTest)

    # WARP_TEST_FILES is expected to be defined from withing the test_folder_name CMakeLists
    set(_WARP_TEST_SOURCES "")
    foreach(_test_file IN LISTS WARP_TEST_FILES)
        list(APPEND _WARP_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${test_folder_name}/${_test_file}")
    endforeach()
    
    # WARP_TEST_INCLUDE_DIRS is expected to be defined from withing the test_folder_name CMakeLists
    set(_WARP_TEST_INCLUDE_DIRS "")
    foreach(_test_include_dir IN LISTS WARP_TEST_INCLUDE_DIRS)
        # we read include directory from root directory of the project
        list(APPEND _WARP_TEST_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/${_test_include_dir}")
    endforeach()
    message(STATUS "Include directories for ${WARP_TEST_NAME} are: ${_WARP_TEST_INCLUDE_DIRS}")

    # WARP_TEST_NAME variables must be set in the included test folder CMakeLists file
    add_executable(${WARP_TEST_NAME})
    target_sources(${WARP_TEST_NAME} PRIVATE ${_WARP_TEST_SOURCES})
    target_include_directories(${WARP_TEST_NAME} PRIVATE ${_WARP_TEST_INCLUDE_DIRS})
    target_link_libraries(${WARP_TEST_NAME} PRIVATE GTest::gtest_main)
    gtest_discover_tests(${WARP_TEST_NAME})
endfunction()

add_warp_test("common_tests")