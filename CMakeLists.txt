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
        ${CMAKE_SOURCE_DIR}/assets/app.rc
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

# NEW: copy assets folder next to binary on every build
add_custom_command(TARGET HPR POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:HPR>/assets
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

    find_library(SLINT_SO
        NAMES slint_cpp
        HINTS "${Slint_DIR}/../.."
              "${Slint_DIR}/../../.."
    )

    if(SLINT_SO)
        add_custom_command(TARGET HPR POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SLINT_SO}"
                $<TARGET_FILE_DIR:HPR>
        )
    else()
        message(WARNING "[HPR] Could not find libslint_cpp.so — you may need to copy it manually next to the binary")
    endif()

    set_target_properties(HPR PROPERTIES
        BUILD_RPATH "$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
    )
endif()