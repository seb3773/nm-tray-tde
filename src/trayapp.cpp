#include "trayapp.h"
#include "traycontroller.h"
#include "nmtray.h"

#include <tdelocale.h>

TrayApp::TrayApp()
    : KUniqueApplication()
    , m_controller(0)
{
    m_controller = new TrayController(this);

    TQString error;
    if (!m_controller->init(&error)) {
        fprintf(stderr, "nm-tray-tde: failed to initialize NM backend: %s\n",
                error.local8Bit().data());
        ::exit(1);
    }

    if (m_controller->tray())
        setMainWidget(m_controller->tray());
}

TrayApp::~TrayApp()
{
    delete m_controller;
}

#include "trayapp.moc"
