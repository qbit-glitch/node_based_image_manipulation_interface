cmake_minimum_required(VERSION 3.15)
project(MyProject)

find_package(OpenCV REQUIRED)

add_executable(MyProject main.cpp)

# Link libraries
target_link_libraries(MyProject PRIVATE ${OpenCV_LIBS})

# Add OpenCV include directories
target_include_directories(MyProject PRIVATE ${OpenCV_INCLUDE_DIRS})

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set rpath so executable can find OpenCV .dylibs
set_target_properties(MyProject PROPERTIES
    INSTALL_RPATH "${OpenCV_INSTALL_PATH}/lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)