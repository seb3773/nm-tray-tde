#include "test-tray.h"

#include <tdeapplication.h>
#include <tdeaboutdata.h>
#include <tdecmdlineargs.h>
#include <tdelocale.h>
#include <kiconloader.h>
#include <tdeglobal.h>
#include <tdepopupmenu.h>

#include <tqtooltip.h>
#include <tqevent.h>
#include <tqpixmap.h>

TestTray::TestTray()
    : KSystemTray(0, "test-tray")
{
    setCaption(i18n("nm-tray-tde test"));
    TQPixmap pix = TDEGlobal::iconLoader()->loadIcon(
        "network-wired", TDEIcon::Panel, TDEIcon::SizeSmall);
    if (!pix.isNull())
        setPixmap(pix);
    TQToolTip::add(this, i18n("nm-tray-tde test-tray"));
    show();
}

void TestTray::mousePressEvent(TQMouseEvent *e)
{
    if (e->button() == TQt::LeftButton) {
        TDEPopupMenu menu;
        menu.insertItem(i18n("test-tray OK"), 0, 0, 0);
        menu.insertSeparator();
        menu.insertItem(i18n("Quit"), tqApp, TQT_SLOT(quit()), 0, 1);
        menu.popup(e->globalPos());
    } else {
        KSystemTray::mousePressEvent(e);
    }
}

#include "test-tray.moc"

extern "C" KDE_EXPORT int kdemain(int argc, char *argv[])
{
    TDEAboutData about("test-tray", I18N_NOOP("test-tray"), "0.1.0");
    TDECmdLineArgs::init(argc, argv, &about);

    TDEApplication app;
    TestTray tray;
    app.setMainWidget(&tray);
    return app.exec();
}
