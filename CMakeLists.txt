cmake_minimum_required(VERSION 3.15)
project(MyProject)

find_package(OpenCV REQUIRED)

# Add ImGui source files
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# Find required packages for ImGui
find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(MyProject main.cpp ${IMGUI_SOURCES})

# Link libraries
target_link_libraries(MyProject PRIVATE 
    ${OpenCV_LIBS}
    glfw
    ${OPENGL_LIBRARIES}
)

# Add include directories
target_include_directories(MyProject PRIVATE 
    ${OpenCV_INCLUDE_DIRS}
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set rpath so executable can find OpenCV .dylibs
set_target_properties(MyProject PROPERTIES
    INSTALL_RPATH "${OpenCV_INSTALL_PATH}/lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)
