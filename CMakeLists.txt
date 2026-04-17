cmake_minimum_required(VERSION 4.1)
project(time_tracker_app)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(time_tracker_app main.cpp)

find_package(Crow CONFIG QUIET)

if (NOT Crow_FOUND)
    include(FetchContent)

    FetchContent_Declare(
            asio
            URL https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-36-0.zip
    )

    FetchContent_MakeAvailable(asio)
    FetchContent_GetProperties(asio)

    if (EXISTS "${asio_SOURCE_DIR}/asio/include/asio.hpp")
        set(ASIO_INCLUDE_DIR "${asio_SOURCE_DIR}/asio/include" CACHE PATH "Path to standalone Asio headers" FORCE)
    elseif (EXISTS "${asio_SOURCE_DIR}/include/asio.hpp")
        set(ASIO_INCLUDE_DIR "${asio_SOURCE_DIR}/include" CACHE PATH "Path to standalone Asio headers" FORCE)
    endif ()

    set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(CROW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(CROW_BUILD_DOCS OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
            Crow
            URL https://github.com/CrowCpp/Crow/archive/refs/tags/v1.3.2.zip
    )

    FetchContent_MakeAvailable(Crow)
endif ()

target_link_libraries(time_tracker_app PRIVATE Crow::Crow)

if (DEFINED crow_SOURCE_DIR)
    target_include_directories(time_tracker_app PRIVATE "${crow_SOURCE_DIR}/include")
endif ()

if (DEFINED ASIO_INCLUDE_DIR)
    target_include_directories(time_tracker_app PRIVATE "${ASIO_INCLUDE_DIR}")
endif ()

if (WIN32)
    target_link_libraries(time_tracker_app PRIVATE ws2_32 mswsock)
endif ()
