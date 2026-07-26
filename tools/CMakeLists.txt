include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${TDE_INCLUDE_DIR}
    ${TQT_INCLUDE_DIRS}
)

set(nm_probe_sources
    nm-probe.cpp
    ../src/nm/nmclient.cpp
    ../src/nm/nmeventpump.cpp
)

tde_automoc(${nm_probe_sources})

add_executable(nm-probe ${nm_probe_sources})

target_compile_definitions(nm-probe PRIVATE NM_NO_SECRET_AGENT=1)

target_include_directories(nm-probe PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${TQT_INCLUDE_DIRS}
    ${LIBNM_INCLUDE_DIRS}
)

target_link_directories(nm-probe PRIVATE ${TQT_LIBRARY_DIRS})

target_link_libraries(nm-probe
    ${TQT_LIBRARIES}
    ${LIBNM_LIBRARIES}
)

set(test_tray_sources
    test-tray.cpp
)

tde_automoc(${test_tray_sources})

tde_add_tdeinit_executable(test-tray AUTOMOC
    SOURCES ${test_tray_sources}
    LINK
        tdecore-shared
        tdeui-shared
)
