cmake_minimum_required(VERSION 3.21)

project(HPR)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Slint QUIET)
if (NOT Slint_FOUND)
  message("Slint could not be located in the CMake module search path. Downloading it from Git and building it locally")
  include(FetchContent)
  FetchContent_Declare(
    Slint
    GIT_REPOSITORY https://github.com/slint-ui/slint.git
    # `release/1` will auto-upgrade to the latest Slint >= 1.0.0 and < 2.0.0
    # `release/1.0` will auto-upgrade to the latest Slint >= 1.0.0 and < 1.1.0
    GIT_TAG release/1
    SOURCE_SUBDIR api/cpp
  )
  FetchContent_MakeAvailable(Slint)
endif (NOT Slint_FOUND)

#No console in windows
if(WIN32)
  add_executable(HPR WIN32
      src/main.cpp
      src/ui-cpp/HPR.cpp
      src/window/getCurrentWindow.cpp
      src/window/validateAndUpdateWindow.cpp
      src/window/windowUtilities.cpp

      #sqLite stuff
      external/sqLite/sqlite3.c
  )

elseif(LINUX)
  add_executable(HPR WIN32
        src/main.cpp
        src/ui-cpp/HPR.cpp
        src/window/getCurrentWindow.cpp
        src/window/validateAndUpdateWindow.cpp
        src/window/windowUtilities.cpp

        #sqLite stuff
        external/sqLite/sqlite3.c
    )

endif()

target_link_libraries(HPR PRIVATE Slint::Slint)

#Recurse the src/ui/*.slint
file(GLOB_RECURSE SLINT_SOURCES src/ui/*.slint)
slint_target_sources(HPR ${SLINT_SOURCES})

target_include_directories(HPR
    PRIVATE
    include/
    external/sqLite/
    external/sqlite_modern_cpp
    external/sqlite_modern_cpp/sqlite_modern_cpp
    external/sqlite_modern_cpp/sqlite_modern_cpp/lists
    external/sqlite_modern_cpp/sqlite_modern_cpp/utility
)
# On Windows, copy the Slint DLL next to the application binary so that it's found.
if (WIN32)
    add_custom_command(TARGET HPR POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:HPR> $<TARGET_FILE_DIR:HPR> COMMAND_EXPAND_LISTS)
endif()
