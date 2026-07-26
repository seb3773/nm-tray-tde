include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${TDE_INCLUDE_DIR}
    ${TQT_INCLUDE_DIRS}
    ${LIBNM_INCLUDE_DIRS}
    ${NOTIFY_INCLUDE_DIRS}
)

link_directories(
    ${TQT_LIBRARY_DIRS}
)

set(nm_tray_sources
    main.cpp
    trayapp.cpp
    traycontroller.cpp
    nmtray.cpp
    traypopup.cpp
    nmdata.cpp
    nmnotifier.cpp
    nmnotifier_glue.c
    hiddenwifidialog.cpp
    createwifidialog.cpp
    connectioneditordialog.cpp
    connectioneditorutil.cpp
    ipv4config.cpp
    wificonnectioneditordialog.cpp
    wiredconnectioneditordialog.cpp
    vpnconnectioneditordialog.cpp
    connectioninfobuilder.cpp
    connectioninfodialog.cpp
    icons.cpp
    wifisecurity.cpp
    wifiutil.cpp
    secretsdialog.cpp
    nm/nmeventpump.cpp
    nm/nmclient.cpp
    nm/nmsecretagent.c
    nm/secrets_prompt.cpp
)

tde_automoc(${nm_tray_sources})

tde_add_executable(nm-tray-tde AUTOMOC
    SOURCES ${nm_tray_sources}
    LINK
        tdecore-shared
        tdeui-shared
        ${LIBNM_LIBRARIES}
        ${NOTIFY_LIBRARIES}
        pthread
    DESTINATION ${BIN_INSTALL_DIR}
)

find_program(SSTRIP_EXECUTABLE sstrip)
if(SSTRIP_EXECUTABLE)
    add_custom_command(TARGET nm-tray-tde POST_BUILD
        COMMAND ${SSTRIP_EXECUTABLE} $<TARGET_FILE:nm-tray-tde>
        COMMENT "Running sstrip on nm-tray-tde"
    )
else()
    add_custom_command(TARGET nm-tray-tde POST_BUILD
        COMMAND strip -s $<TARGET_FILE:nm-tray-tde>
        COMMENT "Running strip on nm-tray-tde"
    )
endif()

install(FILES
    ${CMAKE_SOURCE_DIR}/resources/nm-tray-tde.desktop
    DESTINATION ${XDG_APPS_INSTALL_DIR}
)

install(FILES
    ${CMAKE_SOURCE_DIR}/resources/nm-tray-tde-autostart.desktop
    DESTINATION ${AUTOSTART_INSTALL_DIR}
)
