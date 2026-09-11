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

add_executable(filter_mapping_test
    src/core/FilterMapping.cpp
    tests/filter_mapping_test.cpp
)
target_include_directories(filter_mapping_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(filter_mapping_test PRIVATE Qt6::Core)
add_test(NAME filter_mapping_test COMMAND filter_mapping_test)

add_executable(test_loader_test
    src/core/TestLoader.cpp
    src/core/Logger.cpp
    tests/test_loader_test.cpp
)
target_include_directories(test_loader_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_loader_test PRIVATE Qt6::Core)
add_test(NAME test_loader_test COMMAND test_loader_test)

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

    # ── 模型统计工具：批量扫描目录下 step/igs，统计点线面体拓扑数据 ──
    add_executable(model_stats_dump tools/model_stats_dump.cpp)
    target_include_directories(model_stats_dump PRIVATE ${OpenCASCADE_INCLUDE_DIRS})
    target_link_libraries(model_stats_dump PRIVATE ${OpenCASCADE_LIBRARIES})
    if(MSVC)
        target_compile_options(model_stats_dump PRIVATE /utf-8)
    endif()
    message(STATUS "model_stats_dump enabled")

    # ── 共享拓扑检测工具：统计 TShape 复用/跨体共享 ──
    add_executable(shared_topo_detect tools/shared_topo_detect.cpp)
    target_include_directories(shared_topo_detect PRIVATE ${OpenCASCADE_INCLUDE_DIRS})
    target_link_libraries(shared_topo_detect PRIVATE ${OpenCASCADE_LIBRARIES})
    if(MSVC)
        target_compile_options(shared_topo_detect PRIVATE /utf-8)
    endif()
    message(STATUS "shared_topo_detect enabled")
    # ── 临时探查工具：Geom 句柄级共享 ──
    add_executable(geom_share_probe tools/geom_share_probe.cpp)
    target_include_directories(geom_share_probe PRIVATE ${OpenCASCADE_INCLUDE_DIRS})
    target_link_libraries(geom_share_probe PRIVATE ${OpenCASCADE_LIBRARIES})
    if(MSVC)
        target_compile_options(geom_share_probe PRIVATE /utf-8)
    endif()

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
