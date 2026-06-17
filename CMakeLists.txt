cmake_minimum_required(VERSION 3.25)
project(DropAndDrag VERSION 1.0.0 LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(DROPANDDRAG_BUILD_TESTS "Build unit tests" ON)
option(DROPANDDRAG_ENABLE_SANITIZERS "Enable sanitizers in debug" ON)
option(DROPANDDRAG_ENABLE_LTO "Enable LTO in release" ON)

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
        if(DROPANDDRAG_ENABLE_LTO)
            add_compile_options(-flto)
            add_link_options(-flto)
        endif()
        if(NOT APPLE OR NOT CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
            add_compile_options(-march=native)
        endif()
    endif()
endif()

find_package(SQLite3 REQUIRED)
find_package(Skia REQUIRED)
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
endif()
file(GLOB_RECURSE UI_SOURCES CONFIGURE_DEPENDS
    src/ui/*.cpp
    src/ui/*.mm
)
file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
    src/app/*.cpp
)
set(MAIN_SOURCE src/main.cpp)

add_library(DropAndDragCore STATIC ${CORE_SOURCES})
target_include_directories(DropAndDragCore PUBLIC src src/vendor)
target_link_libraries(DropAndDragCore PUBLIC
    SQLite::SQLite3
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
target_include_directories(DropAndDrag PRIVATE src src/vendor)
target_link_libraries(DropAndDrag PRIVATE
    DropAndDragCore
    DropAndDragPlatform
    DropAndDragUI
    SQLite::SQLite3
    Skia::Skia
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
    )
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
    target_link_libraries(DropAndDragPlatform PRIVATE
        ${CMAKE_DL_LIBS}
    )
    target_link_libraries(DropAndDrag PRIVATE
        ${CMAKE_DL_LIBS}
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
