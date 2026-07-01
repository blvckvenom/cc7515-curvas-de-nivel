# Tarea 3 - Curvas de nivel (CC7515). C++ + GLFW + GLAD.
cmake_minimum_required(VERSION 3.16)
project(curvas_de_nivel LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# GLFW: primero como paquete CMake; si no, via pkg-config (libglfw3-dev).
find_package(glfw3 QUIET)
if(NOT glfw3_FOUND)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GLFW REQUIRED glfw3)
endif()
find_package(OpenGL REQUIRED)

# El ejecutable: nuestro main + el loader GLAD vendorizado.
add_executable(curvas
  src/main.cpp
  third_party/glad/src/glad.c
)

target_include_directories(curvas PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/include
)

# Ruta absoluta a los shaders: asi el binario corre desde cualquier directorio.
target_compile_definitions(curvas PRIVATE
  SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders/")

if(glfw3_FOUND)
  target_link_libraries(curvas PRIVATE glfw OpenGL::GL ${CMAKE_DL_LIBS})
else()
  target_include_directories(curvas PRIVATE ${GLFW_INCLUDE_DIRS})
  target_link_directories(curvas PRIVATE ${GLFW_LIBRARY_DIRS})
  target_link_libraries(curvas PRIVATE ${GLFW_LIBRARIES} OpenGL::GL ${CMAKE_DL_LIBS})
endif()
