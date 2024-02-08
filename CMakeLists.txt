cmake_minimum_required(VERSION 2.6 FATAL_ERROR)

project(DawDemo)

file(GLOB sources ${PROJECT_SOURCE_DIR}/src/*.cc)
file(GLOB headers ${PROJECT_SOURCE_DIR}/inc/*.h)

file(GLOB configurations ${PROJECT_SOURCE_DIR}/config/*.txt)
file(COPY ${configurations} DESTINATION ${PROJECT_BINARY_DIR})

add_executable(daw_exe main.cc ${sources})
target_include_directories(daw_exe PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)

target_link_libraries(daw_exe CAENDigitizer)

add_custom_target(DawDemo DEPENDS daw_exe)
