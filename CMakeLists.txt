cmake_minimum_required(VERSION 3.20)

project(SyneraAutoArena LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_library(synera_core STATIC
    src/core/Bench.cpp
    src/core/Board.cpp
    src/core/EncounterGenerator.cpp
    src/core/GameState.cpp
    src/core/Player.cpp
    src/core/Types.cpp
    src/core/Unit.cpp
    src/core/UnitManager.cpp
)
target_include_directories(synera_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(synera_core PUBLIC cxx_std_20)

add_executable(synera_cli src/main.cpp)
target_link_libraries(synera_cli PRIVATE synera_core)

add_executable(stage1_tests tests/test_stage1.cpp)
target_link_libraries(stage1_tests PRIVATE synera_core)

add_executable(synera_gui
    src/gui/AssetManager.cpp
    src/gui/BenchWidget.cpp
    src/gui/BoardWidget.cpp
    src/gui/DragData.cpp
    src/gui/InspectorPanel.cpp
    src/gui/MainWindow.cpp
    src/gui/PlacementController.cpp
    src/gui/main_gui.cpp
)
target_link_libraries(synera_gui PRIVATE synera_core Qt6::Widgets)
target_compile_definitions(synera_gui PRIVATE SYNERA_PROJECT_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    add_custom_command(TARGET synera_gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/assets"
                "$<TARGET_FILE_DIR:synera_gui>/assets"
        COMMENT "Copying assets next to synera_gui"
    )
endif()
