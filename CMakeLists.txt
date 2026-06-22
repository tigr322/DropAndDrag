cmake_minimum_required(VERSION 3.25)
project(DropAndDrag VERSION 1.0.12 LANGUAGES CXX OBJCXX)

configure_file(src/version.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/version.hpp @ONLY)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(DROPANDDRAG_BUILD_TESTS "Build unit tests" ON)
option(DROPANDDRAG_ENABLE_SANITIZERS "Enable sanitizers in debug" ON)
option(DROPANDDRAG_ENABLE_LTO "Enable LTO in release" ON)
option(DROPANDDRAG_ENABLE_PGO_GENERATE "Build with PGO instrumentation (step 1)" OFF)
option(DROPANDDRAG_ENABLE_PGO_USE "Build with PGO profile data (step 2)" OFF)
set(DROPANDDRAG_PGO_PROFILE "" CACHE PATH "Path to merged .profdata for PGO use")

if(MSVC)
    add_compile_options(/std:c++latest /Zc:preprocessor /W4 /utf-8)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(/Od /Zi)
    else()
        add_compile_options(/O2 /DNDEBUG)
    endif()
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter -Wno-deprecated-declarations -Wno-unused-result)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-Werror)
        add_compile_options(-O0 -g)
        if(DROPANDDRAG_ENABLE_SANITIZERS)
            add_compile_options(-fsanitize=address,undefined)
            add_link_options(-fsanitize=address,undefined)
        endif()
    else()
        add_compile_options(-O3 -DNDEBUG)

        # Hide all symbols by default — enables better inlining across TUs and
        # reduces the export table.  Safe for a monolithic executable.
        add_compile_options(-fvisibility=hidden -fvisibility-inlines-hidden)

        # Dead code elimination.  On macOS the linker flag is -dead_strip;
        # -fdata/function-sections help the ELF linker (Linux) do the same.
        add_compile_options(-fdata-sections -ffunction-sections)

        # ThinLTO: parallel linking, virtually same runtime perf as full LTO,
        # dramatically faster re-links (incremental ThinLTO caches .o IR).
        if(DROPANDDRAG_ENABLE_LTO)
            add_compile_options(-flto=thin)
            add_link_options(-flto=thin)
        endif()

        # CPU targeting.  -march=native doesn't work on Apple arm64 with Apple
        # Clang; -mcpu=native does — it enables AMX, NEON dot-product, and other
        # M-series extensions that -march=armv8 misses. Skip on x86_64 cross.
        if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
            add_compile_options(-mcpu=native)
        elseif(NOT APPLE)
            add_compile_options(-march=native)
        endif()

        # macOS linker: strip unreachable code/data sections.
        # Linux ld: --gc-sections does the same with -ffunction/data-sections above.
        if(APPLE)
            add_link_options(-Wl,-dead_strip)
        elseif(UNIX)
            add_link_options(-Wl,--gc-sections)
        endif()

        # ── PGO ────────────────────────────────────────────────────────────────
        # Step 1: cmake -DDROPANDDRAG_ENABLE_PGO_GENERATE=ON
        #         Run the app (shake, drop 50 files, drag, clear).
        #         llvm-profdata merge -output=default.profdata default.profraw
        # Step 2: cmake -DDROPANDDRAG_ENABLE_PGO_USE=ON
        #                -DDROPANDDRAG_PGO_PROFILE=/path/to/default.profdata
        if(DROPANDDRAG_ENABLE_PGO_GENERATE)
            add_compile_options(-fprofile-instr-generate)
            add_link_options(-fprofile-instr-generate)
        elseif(DROPANDDRAG_ENABLE_PGO_USE)
            if(NOT DROPANDDRAG_PGO_PROFILE)
                message(FATAL_ERROR "DROPANDDRAG_PGO_PROFILE must point to merged .profdata")
            endif()
            add_compile_options(-fprofile-instr-use=${DROPANDDRAG_PGO_PROFILE})
            add_link_options(-fprofile-instr-use=${DROPANDDRAG_PGO_PROFILE})
        endif()
    endif()
endif()

find_package(Skia REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

if(UNIX AND NOT APPLE)
    find_package(OpenGL QUIET)
endif()

file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
    src/core/*.cpp
)
file(GLOB_RECURSE PLATFORM_ABSTRACT_SOURCES CONFIGURE_DEPENDS
    src/platform/*.cpp
)
if(APPLE)
    file(GLOB_RECURSE PLATFORM_IMPL_SOURCES CONFIGURE_DEPENDS
        src/platform_impl/macos/*.mm
        src/platform_impl/macos/*.cpp
    )
elseif(WIN32)
    file(GLOB_RECURSE PLATFORM_IMPL_SOURCES CONFIGURE_DEPENDS
        src/platform_impl/windows/*.cpp
    )
else()
    file(GLOB_RECURSE PLATFORM_IMPL_SOURCES CONFIGURE_DEPENDS
        src/platform_impl/linux/*.cpp
    )
    # drag_drop.cpp handles all DragDropManager methods; drag_drop_linux.cpp
    # duplicates instance() and would cause a linker duplicate-symbol error.
    list(REMOVE_ITEM PLATFORM_IMPL_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/platform_impl/linux/drag_drop_linux.cpp"
    )
    # hotkeys_linux.cpp and tray_linux.cpp provide the full Linux implementations
    # (with XGrabKey / XEMBED).  Keeping the platform-base stubs alongside them
    # would produce duplicate symbol errors.
    list(REMOVE_ITEM PLATFORM_ABSTRACT_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/hotkeys/hotkeys.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/tray/tray.cpp"
    )

    find_package(X11 REQUIRED)
    if(NOT X11_Xi_FOUND)
        message(FATAL_ERROR "XInput2 not found — install with: sudo apt install libxi-dev")
    endif()
    if(X11_Xtst_FOUND)
        message(STATUS "libXtst found — XRecord global mouse tracking enabled")
    else()
        message(STATUS "libXtst not found — install libxtst-dev for XRecord mouse tracking")
    endif()

    # zwp_relative_pointer_manager_v1: delivers relative motion to ALL registered
    # clients regardless of surface focus — works even when cursor is over Nautilus.
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(WAYLAND_CLIENT QUIET wayland-client)
        pkg_check_modules(WAYLAND_PROTOCOLS QUIET wayland-protocols)
        find_program(WAYLAND_SCANNER NAMES wayland-scanner QUIET)
        if(WAYLAND_CLIENT_FOUND AND WAYLAND_PROTOCOLS_FOUND AND WAYLAND_SCANNER)
            pkg_get_variable(WL_PROTO_DIR wayland-protocols pkgdatadir)
            set(_RP_XML "${WL_PROTO_DIR}/unstable/relative-pointer/relative-pointer-unstable-v1.xml")
            if(EXISTS "${_RP_XML}")
                set(_WL_GEN "${CMAKE_CURRENT_BINARY_DIR}/wayland-gen")
                file(MAKE_DIRECTORY "${_WL_GEN}")
                add_custom_command(
                    OUTPUT  "${_WL_GEN}/relative-pointer-unstable-v1.h"
                            "${_WL_GEN}/relative-pointer-unstable-v1.c"
                    COMMAND "${WAYLAND_SCANNER}" client-header "${_RP_XML}"
                            "${_WL_GEN}/relative-pointer-unstable-v1.h"
                    COMMAND "${WAYLAND_SCANNER}" public-code  "${_RP_XML}"
                            "${_WL_GEN}/relative-pointer-unstable-v1.c"
                    DEPENDS "${_RP_XML}" VERBATIM
                )
                # Compile as C++ — project has no C compiler enabled.
                set_source_files_properties(
                    "${_WL_GEN}/relative-pointer-unstable-v1.c"
                    PROPERTIES LANGUAGE CXX
                )
                add_library(WaylandRelPtr STATIC "${_WL_GEN}/relative-pointer-unstable-v1.c")
                target_include_directories(WaylandRelPtr PUBLIC "${_WL_GEN}" ${WAYLAND_CLIENT_INCLUDE_DIRS})
                target_link_libraries(WaylandRelPtr PUBLIC ${WAYLAND_CLIENT_LIBRARIES})
                set(HAVE_WAYLAND_RELPTR ON)
                message(STATUS "Wayland relative pointer enabled — global tracking including Wayland-native windows")
            else()
                message(STATUS "Wayland relative-pointer XML not found; install wayland-protocols")
            endif()
        else()
            message(STATUS "Wayland relative pointer unavailable")
            message(STATUS "  Install: sudo apt install libwayland-dev wayland-protocols libwayland-bin")
        endif()
    endif()
endif()
if(APPLE)
    file(GLOB_RECURSE UI_SOURCES CONFIGURE_DEPENDS
        src/ui/*.cpp
        src/ui/*.mm
    )
else()
    file(GLOB_RECURSE UI_SOURCES CONFIGURE_DEPENDS
        src/ui/*.cpp
    )
endif()
file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
    src/app/*.cpp
)
set(MAIN_SOURCE src/main.cpp)

add_library(DropAndDragCore STATIC ${CORE_SOURCES})
target_include_directories(DropAndDragCore PUBLIC src src/vendor)
target_link_libraries(DropAndDragCore PUBLIC
    Threads::Threads
)

add_library(DropAndDragPlatform STATIC ${PLATFORM_ABSTRACT_SOURCES} ${PLATFORM_IMPL_SOURCES})
target_include_directories(DropAndDragPlatform PUBLIC src src/vendor)
if(APPLE)
    target_compile_options(DropAndDragPlatform PRIVATE -fobjc-arc)
endif()
target_link_libraries(DropAndDragPlatform PUBLIC
    DropAndDragCore
    Threads::Threads
)

add_library(DropAndDragUI STATIC ${UI_SOURCES})
target_include_directories(DropAndDragUI PUBLIC src src/vendor)
if(APPLE)
    target_compile_options(DropAndDragUI PRIVATE -fobjc-arc)
endif()
target_link_libraries(DropAndDragUI PUBLIC
    DropAndDragCore
    Skia::Skia
)

add_executable(DropAndDrag ${MAIN_SOURCE} ${APP_SOURCES})
target_include_directories(DropAndDrag PRIVATE src src/vendor ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(DropAndDrag PRIVATE
    DropAndDragCore
    DropAndDragPlatform
    DropAndDragUI
    Skia::Skia
    ZLIB::ZLIB
    Threads::Threads
)

# UI layer excluded pending Skia API compatibility update
# add_library(DropAndDragUI STATIC ${UI_SOURCES})
# target_link_libraries(DropAndDrag PRIVATE DropAndDragUI)

if(APPLE)
    find_library(COCOA_LIB Cocoa REQUIRED)
    find_library(CARBON_LIB Carbon REQUIRED)
    find_library(QUARTZ_LIB QuartzCore REQUIRED)
    find_library(METAL_LIB Metal REQUIRED)
    find_library(METALKIT_LIB MetalKit REQUIRED)
    find_library(QUICKLOOK_LIB QuickLookThumbnailing REQUIRED)
    target_link_libraries(DropAndDragPlatform PRIVATE
        ${COCOA_LIB}
        ${CARBON_LIB}
        ${QUARTZ_LIB}
        ${METAL_LIB}
        ${METALKIT_LIB}
    )
    target_link_libraries(DropAndDragUI PRIVATE
        ${COCOA_LIB}
        ${QUICKLOOK_LIB}
    )
    target_link_libraries(DropAndDrag PRIVATE
        ${COCOA_LIB}
        ${CARBON_LIB}
        ${QUARTZ_LIB}
        ${METAL_LIB}
        ${METALKIT_LIB}
        ${QUICKLOOK_LIB}
    )
    set_target_properties(DropAndDrag PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/Info.plist"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.dropanddrag.app"
        MACOSX_BUNDLE_BUNDLE_NAME "DropAndDrag"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
    )
    add_custom_command(TARGET DropAndDrag POST_BUILD
        COMMAND mkdir -p "$<TARGET_BUNDLE_CONTENT_DIR:DropAndDrag>/Resources"
        COMMAND cp "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/icon.icns" "$<TARGET_BUNDLE_CONTENT_DIR:DropAndDrag>/Resources/icon.icns"
        COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:DropAndDrag>"
        COMMENT "Copying app icon + ad-hoc signing"
    )
    if(DEFINED ENV{DEVELOPER_ID})
        add_custom_command(TARGET DropAndDrag POST_BUILD
            COMMAND codesign --force --deep --sign "$ENV{DEVELOPER_ID}" --options runtime --timestamp "$<TARGET_BUNDLE_DIR:DropAndDrag>"
        )
    endif()
elseif(WIN32)
    target_link_libraries(DropAndDragPlatform PRIVATE
        dwmapi
        shell32
        ole32
        gdi32
        user32
    )
    target_link_libraries(DropAndDrag PRIVATE
        dwmapi
        shell32
        ole32
        gdi32
        user32
    )
    set_target_properties(DropAndDrag PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
elseif(UNIX AND NOT APPLE)
    target_include_directories(DropAndDragPlatform PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(DropAndDragPlatform PRIVATE
        ${CMAKE_DL_LIBS}
        ${X11_LIBRARIES}
        ${X11_Xi_LIB}
        $<$<BOOL:${X11_Xtst_FOUND}>:${X11_Xtst_LIB}>
    )
    if(X11_Xtst_FOUND)
        target_compile_definitions(DropAndDragPlatform PRIVATE HAVE_XRECORD=1)
    endif()
    if(HAVE_WAYLAND_RELPTR)
        target_compile_definitions(DropAndDragPlatform PRIVATE HAVE_WL_RELATIVE_POINTER=1)
        target_include_directories(DropAndDragPlatform PRIVATE "${_WL_GEN}" ${WAYLAND_CLIENT_INCLUDE_DIRS})
        target_link_libraries(DropAndDragPlatform PRIVATE WaylandRelPtr)
    endif()
    target_link_libraries(DropAndDrag PRIVATE
        ${CMAKE_DL_LIBS}
        ${X11_LIBRARIES}
        ${X11_Xi_LIB}
        $<$<BOOL:${X11_Xtst_FOUND}>:${X11_Xtst_LIB}>
    )
    if(OPENGL_FOUND)
        target_link_libraries(DropAndDrag PRIVATE OpenGL::GL)
    endif()
endif()

install(TARGETS DropAndDrag
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .
)

include(CPack)
set(CPACK_PACKAGE_NAME "DropAndDrag")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "DropAndDrag")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Fast cross-platform drag-and-drop shelf utility")

if(DROPANDDRAG_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
