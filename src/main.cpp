#include <tdeaboutdata.h>
#include <tdecmdlineargs.h>
#include <tdelocale.h>

#include "trayapp.h"

static const char *description = I18N_NOOP(
    "NetworkManager system tray applet for Trinity Desktop");

int main(int argc, char *argv[])
{
    TDEAboutData about(
        "nm-tray-tde",
        I18N_NOOP("nm-tray-tde"),
        "0.1.0",
        description,
        TDEAboutData::License_GPL,
        "Copyright (C) 2026 nm-tray-tde contributors",
        0,
        "https://github.com/palinek/nm-tray");

    about.addAuthor("nm-tray-tde", I18N_NOOP("Developer"), "");

    TDECmdLineArgs::init(argc, argv, &about);

    if (!TrayApp::start())
        return 0;

    TrayApp app;
    app.disableSessionManagement();
    return app.exec();
}
