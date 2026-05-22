cmake_minimum_required(VERSION 3.21)

project(HPR_Premium)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_BUILD_PARALLEL_LEVEL 8)

find_package(Slint QUIET)

#No console in windows
if(WIN32)
  add_executable(HPR_Premium WIN32
		src/main.cpp
        src/tray/trayManager.cpp
        src/database/databaseManager.cpp
		src/appState/appState.cpp
		src/miscellaneous/timeUtils.cpp
        src/miscellaneous/aliasManager.cpp
        src/miscellaneous/configManager.cpp
        src/pattern/patternAnalyzer.cpp
		src/ui-cpp/HPR.cpp
        src/ui-cpp/HPRInterpreter.cpp
        src/ui-cpp/uiEventBridge.cpp
        src/ui-cpp/uiModelManager.cpp
		src/window/getCurrentWindow.cpp
		src/window/validateAndUpdateWindow.cpp
    	src/window/windowUtilities.cpp

        #sqLite stuff
        external/sqLite/sqlite3.c
        ${CMAKE_SOURCE_DIR}/assets/app.rc
  )

elseif(LINUX)
  add_executable(HPR_Premium
        src/main.cpp
        src/tray/trayManager.cpp
        src/database/databaseManager.cpp
        src/appState/appState.cpp
        src/miscellaneous/timeUtils.cpp
        src/miscellaneous/aliasManager.cpp
        src/miscellaneous/configManager.cpp
        src/pattern/patternAnalyzer.cpp
        src/ui-cpp/HPR.cpp
        src/ui-cpp/HPRInterpreter.cpp
        src/ui-cpp/uiEventBridge.cpp
        src/ui-cpp/uiModelManager.cpp
        src/window/getCurrentWindow.cpp
        src/window/validateAndUpdateWindow.cpp
        src/window/windowUtilities.cpp

        #sqLite stuff
        external/sqLite/sqlite3.c
    )

endif()

if (MSVC)
    target_compile_options(HPR_Premium PRIVATE /MP)
endif()

target_link_libraries(HPR_Premium PRIVATE Slint::Slint)

slint_target_sources(HPR_Premium src/ui/app-window.slint)

target_include_directories(HPR_Premium
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
    add_custom_command(TARGET HPR_Premium POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:HPR_Premium> $<TARGET_FILE_DIR:HPR_Premium> COMMAND_EXPAND_LISTS)
endif()

# Copy some aliases.csv to binary path
add_custom_command(TARGET HPR_Premium POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/shippedWithBinary/aliases.csv
    $<TARGET_FILE_DIR:HPR_Premium>/aliases.csv
)

# Copy some tabAliases.csv to binary path
add_custom_command(TARGET HPR_Premium POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/shippedWithBinary/tabAliases.csv
    $<TARGET_FILE_DIR:HPR_Premium>/tabAliases.csv
)

# Copy config.csv to binary path
add_custom_command(TARGET HPR_Premium POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/shippedWithBinary/config.csv
    $<TARGET_FILE_DIR:HPR_Premium>/config.csv
)

# Copy some config installer to binary path
if(UNIX)
    add_custom_command(TARGET HPR_Premium POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/shippedWithBinary/installHPRConfigAndUi.sh
        $<TARGET_FILE_DIR:HPR_Premium>/installHPRConfigAndUi.sh
    )

else()

    # Copy some config installer to binary path, windows
    add_custom_command(TARGET HPR_Premium POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/shippedWithBinary/installHPRConfigAndUi.bat
        $<TARGET_FILE_DIR:HPR_Premium>/installHPRConfigAndUi.bat
    )
endif()

#copy assets folder next to binary on every build
add_custom_command(TARGET HPR_Premium POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:HPR_Premium>/assets
)

#copy ui folder to binary every build
add_custom_command(TARGET HPR_Premium POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/src/ui
    $<TARGET_FILE_DIR:HPR_Premium>/ui
)


if(UNIX)
    add_custom_command(TARGET HPR_Premium POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/shippedWithBinary/installWindowCallsExtension.sh
        $<TARGET_FILE_DIR:HPR_Premium>/installWindowCallsExtension.sh
    )

    find_library(SLINT_SO
        NAMES slint_cpp
        HINTS "${Slint_DIR}/../.."
              "${Slint_DIR}/../../.."
    )

    if(SLINT_SO)
        add_custom_command(TARGET HPR_Premium POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SLINT_SO}"
                $<TARGET_FILE_DIR:HPR_Premium>
        )
    else()
        message(WARNING "[HPR] Could not find libslint_cpp.so — you may need to copy it manually next to the binary")
    endif()

    set_target_properties(HPR_Premium PROPERTIES
        BUILD_RPATH "$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
    )
endif()