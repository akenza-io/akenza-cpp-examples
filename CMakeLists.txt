cmake_minimum_required(VERSION 3.20)
project(ak_mqtt)
add_definitions(-std=c++17)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenSSL REQUIRED)
find_package(jansson CONFIG REQUIRED)
find_path(JWT_CPP_INCLUDE_DIRS "jwt-cpp/base.h")
find_package(PahoMqttCpp CONFIG REQUIRED)
find_package(cxxopts CONFIG REQUIRED)

add_executable(${PROJECT_NAME} src/ak_mqtt.cpp)
set(EXECUTABLE_OUTPUT_PATH "../bin")
target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE jansson::jansson PahoMqttCpp::paho-mqttpp3-static OpenSSL::Crypto OpenSSL::SSL cxxopts::cxxopts)
target_include_directories(${PROJECT_NAME} PRIVATE ${JWT_CPP_INCLUDE_DIRS})