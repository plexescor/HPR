cmake_minimum_required(VERSION 3.21)

project(HPR)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_BUILD_PARALLEL_LEVEL 8)

find_package(Slint QUIET)

#No console in windows
if(WIN32)
  add_executable(HPR WIN32
		src/main.cpp
    src/database/databaseManager.cpp
		src/appState/appState.cpp
		src/miscellaneous/timeUtils.cpp
		src/ui-cpp/HPR.cpp
		src/window/getCurrentWindow.cpp
		src/window/validateAndUpdateWindow.cpp
    	src/window/windowUtilities.cpp

      #sqLite stuff
      external/sqLite/sqlite3.c
  )

elseif(LINUX)
  add_executable(HPR
        src/main.cpp
        src/database/databaseManager.cpp
        src/appState/appState.cpp
        src/miscellaneous/timeUtils.cpp
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

if(UNIX)
    configure_file(
        ${CMAKE_SOURCE_DIR}/shippedWithBinary/installWindowCallsExtension.sh
        ${CMAKE_BINARY_DIR}/installWindowCallsExtension.sh
        COPYONLY
    )
endif()