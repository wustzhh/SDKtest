cmake_minimum_required(VERSION 3.16)
project(test_runner_ui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core OpenGLWidgets)
find_package(VTK QUIET)
find_package(OpenCASCADE QUIET)

# gtest（仅测试目标使用）
enable_testing()
find_package(GTest QUIET)

file(GLOB_RECURSE SOURCES src/*.cpp)
add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Widgets Qt6::Core Qt6::OpenGLWidgets ${CMAKE_DL_LIBS}
)

if(OpenCASCADE_FOUND)
    add_definitions(-DHAS_OCC)
    target_include_directories(${PROJECT_NAME} PRIVATE ${OpenCASCADE_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenCASCADE_LIBRARIES})
    message(STATUS "OCCT enabled")
else()
    message(STATUS "OCCT not found - 3D placeholder")
endif()

# ── 模型加载测试目标：测 StepWorker 模型加载（不依赖 Qt Widgets/OpenGL）──
if(OpenCASCADE_FOUND)
    add_executable(model_load_test
        src/core/StepLoader.cpp
        src/core/Logger.cpp
        tests/model_load_test.cpp
    )
    target_include_directories(model_load_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_definitions(model_load_test PRIVATE HAS_OCC)
    target_link_libraries(model_load_test PRIVATE
        Qt6::Core Qt6::Gui
        ${OpenCASCADE_LIBRARIES}
    )
    message(STATUS "model_load_test enabled")
endif()

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE opengl32)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/config"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/config/test_config.json"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/config/test_config.json"
)
