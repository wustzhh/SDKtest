cmake_minimum_required(VERSION 3.16)
project(test_runner_ui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# ── Qt ──────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS Widgets Core OpenGLWidgets)

# ── Sources ──────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/models/TestResult.h
    src/core/ConfigManager.h      src/core/ConfigManager.cpp
    src/core/TestLoader.h         src/core/TestLoader.cpp
    src/core/TestRunner.h         src/core/TestRunner.cpp
    src/core/ResultParser.h       src/core/ResultParser.cpp
    src/core/XlsxWriter.h         src/core/XlsxWriter.cpp
    src/core/ReportExporter.h     src/core/ReportExporter.cpp
    src/core/Logger.h             src/core/Logger.cpp
    src/core/StepReader.h         src/core/StepReader.cpp
    src/ui/MainWindow.h           src/ui/MainWindow.cpp
    src/ui/Model3DViewer.h        src/ui/Model3DViewer.cpp
    src/ui/TestListPanel.h        src/ui/TestListPanel.cpp
    src/ui/TestProgressPanel.h    src/ui/TestProgressPanel.cpp
    src/ui/ModelRenderView.h      src/ui/ModelRenderView.cpp
    src/ui/ModelInfoPanel.h       src/ui/ModelInfoPanel.cpp
)

# ── Executable ───────────────────────────────────────────
add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::OpenGLWidgets
    ${CMAKE_DL_LIBS}
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE opengl32)
endif()

# ── Copy config beside the exe ──
# $<TARGET_FILE_DIR> 兼容 Ninja 和 VS 两种输出目录
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/config"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/config/test_config.json"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/config/test_config.json"
    COMMENT "Copying config to output directory"
)

# ── Delivery hint ────────────────────────────────────────
message(STATUS "Test Runner UI — build done")
message(STATUS "  Config: ${CMAKE_BINARY_DIR}/Release/config/test_config.json")
message(STATUS "  Edit the config to point at your test binary")
