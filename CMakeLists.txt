cmake_minimum_required(VERSION 3.16)

project(QtStlViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt 6.6.3 requirement
# If Qt6 is not on default path, user can export CMAKE_PREFIX_PATH or Qt6_DIR
find_package(Qt6 6.6 REQUIRED COMPONENTS Widgets 3DCore 3DRender 3DInput 3DExtras)

add_executable(QtStlViewer
    main.cpp
    ProcessModelWithQt3D.h
    ProcessModelWithQt3D.cpp
    ProcessModelWithQt3D.ui
)

target_link_libraries(QtStlViewer PRIVATE
    Qt6::Widgets
    Qt6::3DCore
    Qt6::3DRender
    Qt6::3DInput
    Qt6::3DExtras
)

# Improve warnings on GCC/Clang
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(QtStlViewer PRIVATE -Wall -Wextra -Wpedantic)
endif()