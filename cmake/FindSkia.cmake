include(FindPackageHandleStandardArgs)

set(SKIA_SEARCH_PATHS
    ${SKIA_DIR}
    /usr/local
    /opt/homebrew
    /usr
    ${VCPKG_INSTALLED_DIR}
    ${CONAN_SKIA_ROOT}
)

find_path(SKIA_INCLUDE_DIR
    NAMES include/core/SkCanvas.h
    HINTS ${SKIA_SEARCH_PATHS}
    PATH_SUFFIXES skia
    DOC "Skia include directory"
)

find_library(SKIA_LIBRARY
    NAMES skia libskia skia_static libskia_static
    HINTS ${SKIA_SEARCH_PATHS}
    PATH_SUFFIXES lib out/Static out/Release
    DOC "Skia main library"
)

find_library(SKIA_SKSHAPER_LIBRARY
    NAMES skshaper libskshaper
    HINTS ${SKIA_SEARCH_PATHS}
    PATH_SUFFIXES lib out/Static out/Release
    DOC "Skia text shaping library"
)

find_library(SKIA_SVG_LIBRARY
    NAMES svg libsvg
    HINTS ${SKIA_SEARCH_PATHS}
    PATH_SUFFIXES lib out/Static out/Release
    DOC "Skia SVG library"
)

set(SKIA_GPU_BACKEND_LIBRARIES "")

if(APPLE)
    find_library(SKIA_METAL_LIBRARY
        NAMES skia_metal libskia_metal
        HINTS ${SKIA_SEARCH_PATHS}
        PATH_SUFFIXES lib out/Static out/Release
        DOC "Skia Metal backend library"
    )
    if(SKIA_METAL_LIBRARY)
        list(APPEND SKIA_GPU_BACKEND_LIBRARIES ${SKIA_METAL_LIBRARY})
    endif()
elseif(WIN32)
    find_library(SKIA_DIRECT3D_LIBRARY
        NAMES skia_direct3d libskia_direct3d
        HINTS ${SKIA_SEARCH_PATHS}
        PATH_SUFFIXES lib out/Static out/Release
        DOC "Skia Direct3D backend library"
    )
    if(SKIA_DIRECT3D_LIBRARY)
        list(APPEND SKIA_GPU_BACKEND_LIBRARIES ${SKIA_DIRECT3D_LIBRARY})
    endif()
else()
    find_library(SKIA_GL_LIBRARY
        NAMES skia_gl libskia_gl
        HINTS ${SKIA_SEARCH_PATHS}
        PATH_SUFFIXES lib out/Static out/Release
        DOC "Skia OpenGL backend library"
    )
    if(SKIA_GL_LIBRARY)
        list(APPEND SKIA_GPU_BACKEND_LIBRARIES ${SKIA_GL_LIBRARY})
    endif()
endif()

set(SKIA_REQUIRED_VARS SKIA_INCLUDE_DIR SKIA_LIBRARY)

find_package_handle_standard_args(Skia
    REQUIRED_VARS ${SKIA_REQUIRED_VARS}
    VERSION_VAR SKIA_VERSION
    HANDLE_COMPONENTS
)

if(SKIA_FOUND AND NOT TARGET Skia::Skia)
    set(SKIA_ALL_LIBRARIES ${SKIA_LIBRARY})
    if(SKIA_SKSHAPER_LIBRARY)
        list(APPEND SKIA_ALL_LIBRARIES ${SKIA_SKSHAPER_LIBRARY})
    endif()
    if(SKIA_SVG_LIBRARY)
        list(APPEND SKIA_ALL_LIBRARIES ${SKIA_SVG_LIBRARY})
    endif()
    if(SKIA_GPU_BACKEND_LIBRARIES)
        list(APPEND SKIA_ALL_LIBRARIES ${SKIA_GPU_BACKEND_LIBRARIES})
    endif()

    add_library(Skia::Skia UNKNOWN IMPORTED)
    set_target_properties(Skia::Skia PROPERTIES
        IMPORTED_LOCATION "${SKIA_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SKIA_INCLUDE_DIR};${SKIA_INCLUDE_DIR}/include"
    )

    if(SKIA_ALL_LIBRARIES)
        set_target_properties(Skia::Skia PROPERTIES
            INTERFACE_LINK_LIBRARIES "${SKIA_ALL_LIBRARIES}"
        )
    endif()

    mark_as_advanced(
        SKIA_INCLUDE_DIR
        SKIA_LIBRARY
        SKIA_SKSHAPER_LIBRARY
        SKIA_SVG_LIBRARY
        SKIA_METAL_LIBRARY
        SKIA_DIRECT3D_LIBRARY
        SKIA_GL_LIBRARY
    )
endif()
