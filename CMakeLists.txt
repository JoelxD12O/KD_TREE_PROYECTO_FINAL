cmake_minimum_required(VERSION 3.10)
project(SFMLProject)

find_package(SFML 3.0 REQUIRED COMPONENTS Graphics)

add_executable(sfml-app main.cpp KDTree.cpp Visualizer.cpp)

target_link_libraries(sfml-app PRIVATE SFML::Graphics)
