#
# SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
#
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.31)

project(cardputerzero-sdr VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# CMake configure options

# USE_DESKTOP: whether to build desktop emulator, useful when you want preview with LVGL simulator
# For instance, '-DUSE_DESKTOP=OFF' suggest building for CM0
set(USE_DESKTOP ON CACHE BOOL "Build the desktop emulator target with LVGL SDL support")
set(APP_NAME "${PROJECT_NAME}" CACHE STRING "Application name used for installed asset lookup")
set(APP_ASSETS_ROOT "" CACHE PATH "Optional runtime assets root. Expected subdirectories include fonts/")
set(APP_CONFIG_FILE "" CACHE FILEPATH "Application config file; defaults to the repository file on desktop and /etc/cardputerzero-sdr.conf on device")
set(APP_KEY_INPUT_DEVICE "" CACHE STRING "Optional Linux evdev keyboard/keypad device path, e.g. /dev/input/event0")
set(APP_CARDPUTER_KEY_INPUT_DEVICE "/dev/input/by-path/platform-3f804000.i2c-event" CACHE STRING "Stable Cardputer Zero keyboard path used before evdev scanning")
set(APP_FRAMEBUFFER_DEVICE "/dev/fb0" CACHE STRING "Linux framebuffer device path used by embedded builds")
set(APP_DRM_DEVICE "/dev/dri/card0" CACHE STRING "Linux DRM device path used when APP_USE_DRM=ON")
set(APP_DRM_CONNECTOR_ID "-1" CACHE STRING "DRM connector id used when APP_USE_DRM=ON; -1 auto-selects")
set(APP_USE_DRM OFF CACHE BOOL "Enable LVGL Linux DRM/KMS backend for embedded Linux builds")
set(CM0_ALLOW_FETCH_DEPS ON CACHE BOOL "Allow downloading missing CM0 BSP/dependencies during cross builds")
set(APP_BUNDLE_RTLSDR ON CACHE BOOL "Bundle a pinned librtlsdr runtime in Cardputer Zero packages")
# Export compile commands
# Used for IDE IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile_commands.json" FORCE)

# Keep compiler-provided source locations stable across local and CI build roots.
# This covers __FILE__ strings used by the logger and LVGL assertions.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options("-ffile-prefix-map=${CMAKE_SOURCE_DIR}=.")
endif()

if(NOT APP_CONFIG_FILE)
    if(USE_DESKTOP)
        set(APP_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/config/cardputerzero-sdr.conf")
    else()
        set(APP_CONFIG_FILE "/etc/cardputerzero-sdr.conf")
    endif()
endif()


include(FetchContent)


# lvgl - main ui framework
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS    OFF CACHE BOOL "" FORCE)
set(LV_BUILD_CONF_PATH "${PROJECT_SOURCE_DIR}/src/config/lv_conf.h" CACHE PATH "" FORCE)

# Manually download LVGL dependencies
FetchContent_Declare(
    lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG        v9.5.0
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(lvgl)
if(NOT lvgl_POPULATED)
    if(POLICY CMP0169)
        cmake_policy(PUSH)
        cmake_policy(SET CMP0169 OLD)
    endif()
    FetchContent_Populate(lvgl)
    if(POLICY CMP0169)
        cmake_policy(POP)
    endif()
endif()
add_subdirectory("${lvgl_SOURCE_DIR}" "${lvgl_BINARY_DIR}" EXCLUDE_FROM_ALL)
set(LVGL_SOURCE_DIR "${lvgl_SOURCE_DIR}")
get_filename_component(LVGL_INCLUDE_PARENT "${LVGL_SOURCE_DIR}" DIRECTORY)

# other misc dependencies
# desktop builds
if(USE_DESKTOP)
    message(STATUS "Configuring DESKTOP build")

    # =========================
    # PNG
    # =========================
    find_package(PNG REQUIRED)

    # =========================
    # JPEG (platform abstraction handled by package manager)
    # =========================
    find_package(JPEG REQUIRED)

    if(TARGET libjpeg-turbo::libjpeg-turbo)
        set(JPEG_TARGET libjpeg-turbo::libjpeg-turbo)
    elseif(TARGET JPEG::JPEG)
        set(JPEG_TARGET JPEG::JPEG)
    else()
        message(FATAL_ERROR "No JPEG provider found")
    endif()

    # =========================
    # Freetype
    # =========================

    find_package(Freetype REQUIRED)
    # =========================
    # SDL2
    # =========================
    find_package(SDL2 CONFIG REQUIRED)

    # =========================
    # fmt
    # =========================
    find_package(fmt CONFIG QUIET)
    if(NOT TARGET fmt::fmt)
        message(STATUS "System fmt not found; fetching the pinned source release")
        FetchContent_Declare(
            fmt
            GIT_REPOSITORY https://github.com/fmtlib/fmt.git
            GIT_TAG        12.1.0
            GIT_SHALLOW    TRUE
        )
        set(FMT_INSTALL OFF CACHE BOOL "Disable fmt install rules for app packages" FORCE)
        set(FMT_TEST OFF CACHE BOOL "Disable fmt tests" FORCE)
        set(FMT_DOC OFF CACHE BOOL "Disable fmt docs" FORCE)
        FetchContent_MakeAvailable(fmt)
    endif()

    # =========================
    # ZLIB
    # =========================
    find_package(ZLIB REQUIRED)

    # =========================
    # Link safely
    # =========================
    set(LVGL_DESKTOP_DEPS
        ZLIB::ZLIB
        PNG::PNG
        ${JPEG_TARGET}
        Freetype::Freetype
        SDL2::SDL2
    )

    foreach(dep IN LISTS LVGL_DESKTOP_DEPS)
        if(TARGET ${dep})
            target_link_libraries(lvgl PUBLIC ${dep})
        endif()
    endforeach()

    if(SDL2_FOUND AND NOT TARGET SDL2::SDL2)
        target_include_directories(lvgl PUBLIC ${SDL2_INCLUDE_DIRS})
        target_link_libraries(lvgl PUBLIC ${SDL2_LIBRARIES})
    endif()

    target_compile_definitions(lvgl PUBLIC USE_DESKTOP=1)
    
    if(MSVC)
        target_compile_options(lvgl PUBLIC /utf-8)
    endif()

# Cross builds
# This search individual dependencies from the BSP folder
# and add to the target
# this may not be the good way...
else()
    message(STATUS "Configuring CM0/aarch64 Linux build")

    if(NOT CM0_SDK_ROOT OR NOT EXISTS "${CM0_SDK_ROOT}")
        message(FATAL_ERROR
            "CM0 SDK sysroot not found or invalid.\n"
            "The cp0 toolchain should download the BSP before project() runs, "
            "or configure with -DCM0_SDK_ROOT=<path_to_sdk_sysroot>."
        )
    endif()

    if(CMAKE_SYSROOT)
        set(CM0_SYSROOT "${CMAKE_SYSROOT}")
    else()
        set(CM0_SYSROOT "${CM0_SDK_ROOT}")
    endif()
    if(NOT CM0_MULTIARCH)
        set(CM0_MULTIARCH "aarch64-linux-gnu")
    endif()
    message(STATUS "Using CM0 SDK root: ${CM0_SDK_ROOT}")
    message(STATUS "Using CM0 dependency root: ${CM0_SYSROOT}")

    set(CM0_INCLUDE_HINTS
        "${CM0_SYSROOT}/usr/include/${CM0_MULTIARCH}"
        "${CM0_SYSROOT}/usr/include"
        "${CM0_SYSROOT}/usr/local/include"
        "${CM0_SYSROOT}/include/${CM0_MULTIARCH}"
        "${CM0_SYSROOT}/include"
    )
    set(CM0_LIBRARY_HINTS
        "${CM0_SYSROOT}/usr/lib/${CM0_MULTIARCH}"
        "${CM0_SYSROOT}/usr/lib"
        "${CM0_SYSROOT}/usr/local/lib/${CM0_MULTIARCH}"
        "${CM0_SYSROOT}/usr/local/lib"
        "${CM0_SYSROOT}/lib/${CM0_MULTIARCH}"
        "${CM0_SYSROOT}/lib"
    )

    function(cm0_find_header out_var header_name)
        find_path(${out_var}
            NAMES ${header_name}
            HINTS ${ARGN}
            NO_DEFAULT_PATH
        )
        set(${out_var} "${${out_var}}" PARENT_SCOPE)
    endfunction()

    function(cm0_find_library out_var)
        find_library(${out_var}
            NAMES ${ARGN}
            HINTS ${CM0_LIBRARY_HINTS}
            NO_DEFAULT_PATH
        )
        set(${out_var} "${${out_var}}" PARENT_SCOPE)
    endfunction()

    function(cm0_add_imported_library target_name library_path include_dirs)
        if(NOT TARGET ${target_name})
            add_library(${target_name} UNKNOWN IMPORTED)
            set_target_properties(${target_name} PROPERTIES
                IMPORTED_LOCATION "${library_path}"
                INTERFACE_INCLUDE_DIRECTORIES "${include_dirs}"
            )
        endif()
    endfunction()

    # Display backend, DRM is optional. All lookups stay inside the BSP sysroot.
    if(APP_USE_DRM)
        cm0_find_header(DRM_XF86_INCLUDE_DIR xf86drm.h
            "${CM0_SYSROOT}/usr/include"
            "${CM0_SYSROOT}/usr/include/libdrm"
        )
        cm0_find_header(DRM_XF86_MODE_INCLUDE_DIR xf86drmMode.h
            "${CM0_SYSROOT}/usr/include"
            "${CM0_SYSROOT}/usr/include/libdrm"
        )
        cm0_find_header(DRM_MODE_INCLUDE_DIR drm.h
            "${CM0_SYSROOT}/usr/include/drm"
            "${CM0_SYSROOT}/usr/include/libdrm"
            "${CM0_SYSROOT}/usr/include"
        )
        cm0_find_library(DRM_LIBRARY drm drm.so.2)

        if(NOT DRM_XF86_INCLUDE_DIR
           OR NOT DRM_XF86_MODE_INCLUDE_DIR
           OR NOT DRM_MODE_INCLUDE_DIR
           OR NOT DRM_LIBRARY)
            message(FATAL_ERROR
                "APP_USE_DRM=ON but libdrm headers/library were not found in BSP sysroot.\n"
                "Provide them in ${CM0_SYSROOT} or disable with -DAPP_USE_DRM=OFF."
            )
        endif()
        cm0_add_imported_library(CM0::DRM "${DRM_LIBRARY}"
            "${DRM_XF86_INCLUDE_DIR};${DRM_XF86_MODE_INCLUDE_DIR};${DRM_MODE_INCLUDE_DIR}"
        )
        message(STATUS "CM0 DRM display support enabled")
    endif()

    cm0_find_header(FREETYPE_INCLUDE_DIR ft2build.h
        "${CM0_SYSROOT}/usr/include/freetype2"
        "${CM0_SYSROOT}/usr/include"
    )
    cm0_find_library(FREETYPE_LIBRARY freetype)
    if(NOT FREETYPE_INCLUDE_DIR OR NOT FREETYPE_LIBRARY)
        message(FATAL_ERROR "Missing Freetype header/library in BSP sysroot: ${CM0_SYSROOT}")
    endif()
    cm0_add_imported_library(CM0::Freetype "${FREETYPE_LIBRARY}" "${FREETYPE_INCLUDE_DIR}")

    cm0_find_header(PNG_INCLUDE_DIR png.h
        "${CM0_SYSROOT}/usr/include/libpng16"
        "${CM0_SYSROOT}/usr/include/libpng"
        "${CM0_SYSROOT}/usr/include"
    )
    cm0_find_library(PNG_LIBRARY png16 png)
    if(NOT PNG_INCLUDE_DIR OR NOT PNG_LIBRARY)
        message(FATAL_ERROR "Missing PNG header/library in BSP sysroot: ${CM0_SYSROOT}")
    endif()
    cm0_add_imported_library(CM0::PNG "${PNG_LIBRARY}" "${PNG_INCLUDE_DIR}")

    cm0_find_header(JPEG_INCLUDE_DIR jpeglib.h
        "${CM0_SYSROOT}/usr/include"
    )
    cm0_find_library(JPEG_LIBRARY jpeg)
    if(NOT JPEG_INCLUDE_DIR OR NOT JPEG_LIBRARY)
        message(FATAL_ERROR "Missing JPEG header/library in BSP sysroot: ${CM0_SYSROOT}")
    endif()
    cm0_add_imported_library(CM0::JPEG "${JPEG_LIBRARY}" "${JPEG_INCLUDE_DIR}")

    cm0_find_header(ZLIB_INCLUDE_DIR zlib.h
        "${CM0_SYSROOT}/usr/include"
    )
    cm0_find_library(ZLIB_LIBRARY z)
    if(NOT ZLIB_INCLUDE_DIR OR NOT ZLIB_LIBRARY)
        message(FATAL_ERROR "Missing ZLIB header/library in BSP sysroot: ${CM0_SYSROOT}")
    endif()
    cm0_add_imported_library(ZLIB::ZLIB "${ZLIB_LIBRARY}" "${ZLIB_INCLUDE_DIR}")

    set(CM0_MULTIARCH_INCLUDE_DIRS)
    foreach(CM0_INCLUDE_DIR IN ITEMS
            "${CM0_SYSROOT}/usr/include/${CM0_MULTIARCH}"
            "${CM0_SYSROOT}/include/${CM0_MULTIARCH}")
        if(EXISTS "${CM0_INCLUDE_DIR}/bits/timesize.h" OR EXISTS "${CM0_INCLUDE_DIR}/bits/wordsize.h")
            list(APPEND CM0_MULTIARCH_INCLUDE_DIRS "${CM0_INCLUDE_DIR}")
            message(STATUS "Found multiarch include: ${CM0_INCLUDE_DIR}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES CM0_MULTIARCH_INCLUDE_DIRS)

    cm0_find_header(FMT_INCLUDE_DIR fmt/format.h
        "${CM0_SYSROOT}/usr/include"
    )
    cm0_find_library(FMT_LIBRARY fmt libfmt.so.10 libfmt.so.10.1.0)
    if(FMT_INCLUDE_DIR AND FMT_LIBRARY)
        cm0_add_imported_library(fmt::fmt "${FMT_LIBRARY}" "${FMT_INCLUDE_DIR}")
    elseif(CM0_ALLOW_FETCH_DEPS)
        message(STATUS "Missing fmt in BSP sysroot, fetching from GitHub because CM0_ALLOW_FETCH_DEPS=ON")
        FetchContent_Declare(
            fmt
            GIT_REPOSITORY https://github.com/fmtlib/fmt.git
            GIT_TAG        10.1.1
            GIT_SHALLOW    TRUE
        )
        set(FMT_INSTALL OFF CACHE BOOL "Disable fmt install rules for app packages" FORCE)
        set(FMT_TEST OFF CACHE BOOL "Disable fmt tests" FORCE)
        set(FMT_DOC OFF CACHE BOOL "Disable fmt docs" FORCE)
        FetchContent_MakeAvailable(fmt)
        if(TARGET fmt)
            target_include_directories(fmt SYSTEM BEFORE PRIVATE ${CM0_MULTIARCH_INCLUDE_DIRS})
        endif()
        if(TARGET fmt AND NOT TARGET fmt::fmt)
            add_library(fmt::fmt ALIAS fmt)
        endif()
    else()
        message(FATAL_ERROR
            "Missing fmt header/library in BSP sysroot: ${CM0_SYSROOT}\n"
            "Install/copy fmt into the BSP, or reconfigure with -DCM0_ALLOW_FETCH_DEPS=ON."
        )
    endif()

    set_target_properties(CM0::PNG PROPERTIES INTERFACE_LINK_LIBRARIES ZLIB::ZLIB)
    set_target_properties(CM0::Freetype PROPERTIES INTERFACE_LINK_LIBRARIES "CM0::PNG;ZLIB::ZLIB")

    cm0_find_library(PTHREAD_LIBRARY pthread libpthread.so.0 libpthread.a)
    cm0_find_library(M_LIBRARY m libm.so libm.so.6 libm.a)
    cm0_find_library(USB_LIBRARY usb-1.0 libusb-1.0.so.0)
    if(NOT PTHREAD_LIBRARY)
        message(FATAL_ERROR "Missing pthread library in BSP sysroot: ${CM0_SYSROOT}")
    endif()
    if(NOT M_LIBRARY)
        message(FATAL_ERROR "Missing math library in BSP sysroot: ${CM0_SYSROOT}")
    endif()
    if(APP_BUNDLE_RTLSDR AND NOT USB_LIBRARY)
        message(FATAL_ERROR "Bundled RTL-SDR requires libusb-1.0 in BSP sysroot: ${CM0_SYSROOT}")
    endif()

    set(PLATFORM_INCLUDE_DIR ${CM0_MULTIARCH_INCLUDE_DIRS})
    set(PLATFORM_LIBS ${PTHREAD_LIBRARY} ${M_LIBRARY})

    set(LVGL_INCLUDE_DIR
        ${FREETYPE_INCLUDE_DIR}
        ${PNG_INCLUDE_DIR}
        ${JPEG_INCLUDE_DIR}
        ${ZLIB_INCLUDE_DIR}
    )
    set(LVGL_LIBS
        CM0::Freetype
        CM0::PNG
        CM0::JPEG
        ZLIB::ZLIB
    )
    if(APP_USE_DRM)
        list(APPEND LVGL_INCLUDE_DIR
            ${DRM_XF86_INCLUDE_DIR}
            ${DRM_XF86_MODE_INCLUDE_DIR}
            ${DRM_MODE_INCLUDE_DIR}
        )
        list(APPEND LVGL_LIBS CM0::DRM)
    endif()

    list(REMOVE_DUPLICATES PLATFORM_INCLUDE_DIR)
    list(REMOVE_DUPLICATES PLATFORM_LIBS)
    list(REMOVE_DUPLICATES LVGL_INCLUDE_DIR)
    list(REMOVE_DUPLICATES LVGL_LIBS)

    target_compile_definitions(lvgl
        PUBLIC
            USE_DESKTOP=0
            APP_USE_DRM=$<BOOL:${APP_USE_DRM}>
    )

    # fix: issue of compiling lvgl on gcc >= 15.0
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
        # GCC version is available in CMAKE_C_COMPILER_VERSION
        if (CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 15.0)
            target_compile_options(lvgl PRIVATE -Wno-stringop-overflow)
        endif()
    endif()

    target_include_directories(lvgl
        PUBLIC
            ${LVGL_INCLUDE_DIR}
            ${PLATFORM_INCLUDE_DIR}
    )

    target_link_libraries(lvgl
        PUBLIC
            ${LVGL_LIBS}
            ${PLATFORM_LIBS}
    )

endif()

# The official BSP includes libusb at runtime but not librtlsdr. Device
# packages carry a pinned GPL shared library in an app-private directory.
if(NOT USE_DESKTOP AND APP_BUNDLE_RTLSDR)
    # CMake 3.x permits the explicit populate-only pattern below. CMake 4.x
    # gates it behind CMP0169; using OLD here keeps third-party projects out of
    # our build while retaining compatibility with the AppBuilder toolchain.
    if(POLICY CMP0169)
        cmake_policy(PUSH)
        cmake_policy(SET CMP0169 OLD)
    endif()
    FetchContent_Declare(
        rtlsdr_source
        URL      https://github.com/osmocom/rtl-sdr/archive/refs/tags/v2.0.3.tar.gz
        URL_HASH SHA256=23dec530f40456a8396e7c897f313859877e18b4c6432c6d323645871e351f6f
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    )
    FetchContent_Declare(
        libusb_headers
        URL      https://github.com/libusb/libusb/archive/refs/tags/v1.0.29.tar.gz
        URL_HASH SHA256=7c2dd39c0b2589236e48c93247c986ae272e27570942b4163cb00a060fcf1b74
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    )
    FetchContent_GetProperties(rtlsdr_source)
    if(NOT rtlsdr_source_POPULATED)
        FetchContent_Populate(rtlsdr_source)
    endif()
    FetchContent_GetProperties(libusb_headers)
    if(NOT libusb_headers_POPULATED)
        FetchContent_Populate(libusb_headers)
    endif()
    if(POLICY CMP0169)
        cmake_policy(POP)
    endif()

    add_library(bundled_rtlsdr SHARED
        "${rtlsdr_source_SOURCE_DIR}/src/librtlsdr.c"
        "${rtlsdr_source_SOURCE_DIR}/src/tuner_e4k.c"
        "${rtlsdr_source_SOURCE_DIR}/src/tuner_fc0012.c"
        "${rtlsdr_source_SOURCE_DIR}/src/tuner_fc0013.c"
        "${rtlsdr_source_SOURCE_DIR}/src/tuner_fc2580.c"
        "${rtlsdr_source_SOURCE_DIR}/src/tuner_r82xx.c"
    )
    target_include_directories(bundled_rtlsdr PRIVATE
        "${rtlsdr_source_SOURCE_DIR}/include"
        "${libusb_headers_SOURCE_DIR}/libusb"
    )
    target_compile_definitions(bundled_rtlsdr PRIVATE rtlsdr_EXPORTS DETACH_KERNEL_DRIVER=1)
    target_compile_options(bundled_rtlsdr PRIVATE
        -Wall -Wextra -Wno-unused-parameter -Wno-unused -Wno-sign-compare
        -Wno-declaration-after-statement -fvisibility=hidden
    )
    target_link_libraries(bundled_rtlsdr PRIVATE "${USB_LIBRARY}" "${PTHREAD_LIBRARY}")
    set_target_properties(bundled_rtlsdr PROPERTIES
        OUTPUT_NAME rtlsdr
        SOVERSION 0
        VERSION 2.0.3
    )
endif()



# ====================================================
# Platform sources
# ====================================================
# Create application platform sources and static library
file(GLOB_RECURSE PLATFORM_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/*.cpp"
)

add_library(${PROJECT_NAME}_platform 
    STATIC
    ${PLATFORM_SOURCES}
)

target_include_directories(${PROJECT_NAME}_platform
    PUBLIC
        ${LVGL_INCLUDE_DIR}
        "${CMAKE_CURRENT_SOURCE_DIR}/src/platform"
)

target_compile_definitions(${PROJECT_NAME}_platform
    PUBLIC
        APP_KEY_INPUT_DEVICE="${APP_KEY_INPUT_DEVICE}"
        APP_CARDPUTER_KEY_INPUT_DEVICE="${APP_CARDPUTER_KEY_INPUT_DEVICE}"
        APP_FRAMEBUFFER_DEVICE="${APP_FRAMEBUFFER_DEVICE}"
        APP_DRM_DEVICE="${APP_DRM_DEVICE}"
        APP_DRM_CONNECTOR_ID=${APP_DRM_CONNECTOR_ID}
)

target_link_libraries(${PROJECT_NAME}_platform
    PUBLIC
        lvgl
)

# ====================================================
# Radio device sources (runtime-loaded; no hard librtlsdr link dependency)
# ====================================================
file(GLOB_RECURSE AUDIO_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/audio/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/audio/*.cpp"
)

add_library(${PROJECT_NAME}_audio STATIC ${AUDIO_SOURCES})
target_include_directories(${PROJECT_NAME}_audio PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/audio"
)
target_link_libraries(${PROJECT_NAME}_audio PRIVATE ${CMAKE_DL_LIBS})

file(GLOB_RECURSE DEVICE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/device/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/device/*.cpp"
)

add_library(${PROJECT_NAME}_device STATIC ${DEVICE_SOURCES})
target_include_directories(${PROJECT_NAME}_device PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/device"
)
target_link_libraries(${PROJECT_NAME}_device PRIVATE ${CMAKE_DL_LIBS})

# ====================================================
# Model sources
# ====================================================
file(GLOB_RECURSE MODEL_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/model/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/model/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/i18n/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/i18n/*.cpp"
)

add_library(${PROJECT_NAME}_model
    STATIC
    ${MODEL_SOURCES}
)

target_link_libraries(${PROJECT_NAME}_device PUBLIC ${PROJECT_NAME}_model ${PROJECT_NAME}_audio)

target_include_directories(${PROJECT_NAME}_model
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src/model"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/i18n"
)

# ====================================================
# View sources
# ====================================================
file(GLOB_RECURSE REACTIVE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/reactive/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/reactive/*.cpp"
)

add_library(${PROJECT_NAME}_reactive
    STATIC
    ${REACTIVE_SOURCES}
)

target_include_directories(${PROJECT_NAME}_reactive
    PUBLIC
        ${LVGL_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src/reactive
        ${CMAKE_CURRENT_SOURCE_DIR}/src/view
)

target_link_libraries(${PROJECT_NAME}_reactive
    PUBLIC
        lvgl
)

file(GLOB_RECURSE UI_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/view/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/view/*.cpp"  
)

add_library(${PROJECT_NAME}_view
    STATIC
    ${UI_SOURCES}
)

target_include_directories(${PROJECT_NAME}_view
    PUBLIC
        ${LVGL_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src/view
        ${CMAKE_CURRENT_SOURCE_DIR}/src/view/screens
        ${CMAKE_CURRENT_SOURCE_DIR}/src/view/widgets
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app
        ${CMAKE_CURRENT_SOURCE_DIR}/src/reactive
        ${CMAKE_CURRENT_SOURCE_DIR}/src/viewmodel
        ${CMAKE_CURRENT_SOURCE_DIR}/src/model
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/i18n
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform
)

target_link_libraries(${PROJECT_NAME}_view
    PUBLIC
        lvgl
        ${PROJECT_NAME}_reactive
        ${PROJECT_NAME}_platform
        ${PROJECT_NAME}_device
        ${PROJECT_NAME}_audio
)

# ====================================================
# ViewModel sources
# ====================================================
file(GLOB_RECURSE VIEWMODEL_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/viewmodel/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/viewmodel/*.cpp"
)

add_library(${PROJECT_NAME}_viewmodel
    STATIC
    ${VIEWMODEL_SOURCES}
)

target_include_directories(${PROJECT_NAME}_viewmodel
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src/viewmodel/"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/model"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/i18n"
        ${LVGL_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME}_viewmodel
    PUBLIC
        lvgl
        ${PROJECT_NAME}_reactive
        ${PROJECT_NAME}_model
        ${PROJECT_NAME}_platform
        ${PROJECT_NAME}_device
)


# ====================================================
#   Core sources
# - Application lifecycle management
# - UI screen management
# - UI asset management
# - Reactive framework based on lvgl observer mode
# - Logger based on fmt
# ====================================================
file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/app/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/app/*.cpp"
)

file(GLOB_RECURSE LOGGER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/logger/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/logger/*.cpp"
)

add_library(logger STATIC
    ${LOGGER_SOURCES}
)

target_include_directories(logger PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/logger
    ${PLATFORM_INCLUDE_DIR}
)

target_link_libraries(logger
    PUBLIC
        fmt::fmt
    PRIVATE
        ${PLATFORM_LIBS}
)

add_library(${PROJECT_NAME}_core STATIC
    ${APP_SOURCES}
)

target_include_directories(${PROJECT_NAME}_core
    PUBLIC
        ${LVGL_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app
        ${CMAKE_CURRENT_SOURCE_DIR}/src/logger
        ${CMAKE_CURRENT_SOURCE_DIR}/src/view/screens
        ${CMAKE_CURRENT_SOURCE_DIR}/src/view/widgets
        ${CMAKE_CURRENT_SOURCE_DIR}/src/viewmodel
        ${CMAKE_CURRENT_SOURCE_DIR}/src/model
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/i18n
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform
)

target_compile_definitions(${PROJECT_NAME}_core
    PUBLIC
        APP_NAME="${APP_NAME}"
        APP_CONFIG_FILE="${APP_CONFIG_FILE}"
        APP_CMAKE_ASSETS_ROOT="${APP_ASSETS_ROOT}"
        APP_DRM_DEVICE="${APP_DRM_DEVICE}"
        APP_DRM_CONNECTOR_ID=${APP_DRM_CONNECTOR_ID}
)

if(USE_DESKTOP)
    target_compile_definitions(${PROJECT_NAME}_core
        PUBLIC APP_SOURCE_ASSETS_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/assets"
    )
else()
    # Device packages use /usr/share/${APP_NAME}; do not embed the transient build root.
    target_compile_definitions(${PROJECT_NAME}_core
        PUBLIC APP_SOURCE_ASSETS_ROOT=""
    )
endif()

target_link_libraries(${PROJECT_NAME}_core
    PUBLIC
        lvgl
        logger
        ${PROJECT_NAME}_view
        ${PROJECT_NAME}_viewmodel
        ${PROJECT_NAME}_platform
)

if(USE_DESKTOP)
    target_link_libraries(${PROJECT_NAME}_core
        PRIVATE
            PNG::PNG
    )
    if(TARGET SDL2::SDL2)
        target_link_libraries(${PROJECT_NAME}_core PRIVATE SDL2::SDL2)
    else()
        target_include_directories(${PROJECT_NAME}_core PRIVATE ${SDL2_INCLUDE_DIRS})
        target_link_libraries(${PROJECT_NAME}_core PRIVATE ${SDL2_LIBRARIES})
    endif()
endif()

# ====================================================
# main
# ====================================================
add_executable(${PROJECT_NAME}
    src/main.cpp
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        ${PROJECT_NAME}_platform
        ${PROJECT_NAME}_device
        ${PROJECT_NAME}_core
        ${PROJECT_NAME}_model
        ${PROJECT_NAME}_view
        ${PROJECT_NAME}_viewmodel
)

include(CTest)
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_executable(radio_model_test tests/radio_model_test.cpp)
    target_link_libraries(radio_model_test PRIVATE ${PROJECT_NAME}_model)
    add_test(NAME radio_model COMMAND radio_model_test)

    add_executable(input_policy_test tests/input_policy_test.cpp)
    target_include_directories(input_policy_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/platform")
    add_test(NAME input_policy COMMAND input_policy_test)

    add_executable(display_device_test
        tests/display_device_test.cpp
        src/platform/display_device.cpp
    )
    target_include_directories(display_device_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/platform")
    add_test(NAME display_device COMMAND display_device_test)

    add_executable(synthetic_spectrum_test tests/synthetic_spectrum_test.cpp)
    target_link_libraries(synthetic_spectrum_test PRIVATE ${PROJECT_NAME}_model)
    add_test(NAME synthetic_spectrum COMMAND synthetic_spectrum_test)

    add_executable(iq_spectrum_test tests/iq_spectrum_test.cpp)
    target_link_libraries(iq_spectrum_test PRIVATE ${PROJECT_NAME}_model)
    add_test(NAME iq_spectrum COMMAND iq_spectrum_test)

    add_executable(wfm_demodulator_test tests/wfm_demodulator_test.cpp)
    target_link_libraries(wfm_demodulator_test PRIVATE ${PROJECT_NAME}_model)
    add_test(NAME wfm_demodulator COMMAND wfm_demodulator_test)

    add_executable(translations_test tests/translations_test.cpp)
    target_link_libraries(translations_test PRIVATE ${PROJECT_NAME}_model)
    add_test(NAME translations COMMAND translations_test)

    add_executable(application_config_test
        tests/application_config_test.cpp
        src/app/application_config.cpp
    )
    target_include_directories(application_config_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/app")
    add_test(NAME application_config
             COMMAND application_config_test "${CMAKE_CURRENT_BINARY_DIR}/application-config-test")

    add_library(fake_rtlsdr SHARED tests/fake_rtlsdr.cpp)
    set_target_properties(fake_rtlsdr PROPERTIES PREFIX "" OUTPUT_NAME "fake_rtlsdr")

    add_library(fake_asound SHARED tests/fake_asound.cpp)
    set_target_properties(fake_asound PROPERTIES PREFIX "" OUTPUT_NAME "fake_asound")

    add_executable(alsa_audio_sink_test tests/alsa_audio_sink_test.cpp)
    target_link_libraries(alsa_audio_sink_test PRIVATE ${PROJECT_NAME}_audio)
    add_test(NAME alsa_audio_sink
             COMMAND alsa_audio_sink_test $<TARGET_FILE:fake_asound>)

    add_executable(rtl_sdr_device_test tests/rtl_sdr_device_test.cpp)
    target_link_libraries(rtl_sdr_device_test PRIVATE ${PROJECT_NAME}_device)
    add_test(NAME rtl_sdr_device
             COMMAND rtl_sdr_device_test $<TARGET_FILE:fake_rtlsdr>)

    add_executable(radio_session_test tests/radio_session_test.cpp)
    target_link_libraries(radio_session_test PRIVATE ${PROJECT_NAME}_device)
    add_test(NAME radio_session
             COMMAND radio_session_test $<TARGET_FILE:fake_rtlsdr> $<TARGET_FILE:fake_asound>)

    find_package(Python3 3.9 COMPONENTS Interpreter REQUIRED)
    add_test(NAME diagnostics_log
             COMMAND ${Python3_EXECUTABLE}
                     "${CMAKE_CURRENT_SOURCE_DIR}/tests/diagnostics_log_test.py"
                     "${CMAKE_CURRENT_SOURCE_DIR}/scripts/summarize_diagnostics.py")
    add_test(NAME device_p0_tool
             COMMAND ${Python3_EXECUTABLE}
                     "${CMAKE_CURRENT_SOURCE_DIR}/tests/device_p0_tool_test.py"
                     "${CMAKE_CURRENT_SOURCE_DIR}/scripts/cardputerzero-sdr-p0")
    if(UNIX)
        add_test(NAME graceful_shutdown
                 COMMAND ${Python3_EXECUTABLE}
                         "${CMAKE_CURRENT_SOURCE_DIR}/tests/graceful_shutdown_test.py"
                         $<TARGET_FILE:${PROJECT_NAME}>)
    endif()
endif()

# Package deb
include(cmake/cm0-package.cmake)
