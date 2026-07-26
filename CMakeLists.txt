################################################
# nm-tray-tde - NetworkManager tray for Trinity
################################################
cmake_minimum_required(VERSION 3.10)
project(nm-tray-tde C CXX)

if(NOT DEFINED TDE_PREFIX)
    set(TDE_PREFIX "/opt/trinity")
endif()

list(APPEND CMAKE_MODULE_PATH "${TDE_PREFIX}/share/cmake/modules")

include(TDESetupPaths)
tde_setup_paths()
include(TDEMacros)
include(FindPkgConfig)

find_package(TQt REQUIRED)
find_package(TDE REQUIRED)

pkg_check_modules(LIBNM REQUIRED libnm)
pkg_check_modules(NOTIFY REQUIRED libnotify)

tde_setup_architecture_flags()
tde_setup_largefiles()

add_definitions(-DHAVE_CONFIG_H -UTQT_NO_COMPAT)

# Aggressive size optimization
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os -flto=auto -fstrict-aliasing -ffunction-sections -fdata-sections -fno-ident -fno-plt -fomit-frame-pointer -fno-stack-protector -fno-math-errno -fvisibility=hidden -fno-threadsafe-statics -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables -Wno-deprecated-declarations")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Os -flto=auto -fstrict-aliasing -ffunction-sections -fdata-sections -fno-ident -fno-plt -fomit-frame-pointer -fno-stack-protector -fno-math-errno -fvisibility=hidden -fno-unwind-tables -fno-asynchronous-unwind-tables -Wno-deprecated-declarations")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-O1 -Wl,--gc-sections -Wl,--as-needed -s")

add_subdirectory(src)
