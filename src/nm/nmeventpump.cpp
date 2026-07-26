#include "nmeventpump.h"
#include "glib_compat.h"

#include <tdeapplication.h>

NmEventPump::NmEventPump(int intervalMs, TQObject *parent)
    : TQObject(parent)
{
    connect(&m_timer, TQT_SIGNAL(timeout()), this, TQT_SLOT(onTimeout()));
    m_timer.start(intervalMs);
}

void NmEventPump::pump()
{
    while (g_main_context_pending(NULL))
        g_main_context_iteration(NULL, FALSE);
}

void NmEventPump::pumpAfterAsync()
{
    pump();
}

void NmEventPump::pumpRepeated(int times)
{
    if (times <= 0)
        return;

    for (int i = 0; i < times; ++i) {
        pump();
        g_usleep(1000);
    }
}

void NmEventPump::pumpUi()
{
    pump();
    if (tqApp)
        tqApp->processEvents();
}

void NmEventPump::onTimeout()
{
    pump();
}

#include "nmeventpump.moc"
