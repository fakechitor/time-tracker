cmake_minimum_required(VERSION 4.1)
project(time_tracker_app)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

find_package(Crow CONFIG QUIET)

if (NOT Crow_FOUND)
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

FetchContent_Declare(
        picosha2
        URL https://github.com/okdshin/PicoSHA2/archive/27fcf6979298949e8a462e16d09a0351c18fcaf2.zip
)
FetchContent_MakeAvailable(picosha2)
FetchContent_GetProperties(picosha2)

add_executable(time_tracker_app
        main.cpp
        src/app/auth/user_store.cpp
        src/app/auth/jwt_service.cpp
        src/app/auth/auth_service.cpp
        src/app/auth/auth_middleware.cpp
        src/app/db/postgres.cpp
        src/app/routes/auth_routes.cpp
        src/app/routes/system_routes.cpp
)

target_link_libraries(time_tracker_app PRIVATE Crow::Crow)

if (WIN32)
    target_link_libraries(time_tracker_app PRIVATE ws2_32 mswsock)
endif ()

target_include_directories(time_tracker_app PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

if (DEFINED crow_SOURCE_DIR)
    target_include_directories(time_tracker_app PRIVATE "${crow_SOURCE_DIR}/include")
endif ()

if (DEFINED ASIO_INCLUDE_DIR)
    target_include_directories(time_tracker_app PRIVATE "${ASIO_INCLUDE_DIR}")
endif ()

if (DEFINED picosha2_SOURCE_DIR)
    target_include_directories(time_tracker_app PRIVATE "${picosha2_SOURCE_DIR}")
endif ()
