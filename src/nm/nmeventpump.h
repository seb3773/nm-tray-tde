#ifndef NM_EVENT_PUMP_H
#define NM_EVENT_PUMP_H

#include <tqobject.h>
#include <tqtimer.h>

class NmEventPump : public TQObject
{
    TQ_OBJECT

public:
    explicit NmEventPump(int intervalMs = 30, TQObject *parent = 0);
    static void pump();
    static void pumpAfterAsync();
    static void pumpRepeated(int times);
    static void pumpUi();

private slots:
    void onTimeout();

private:
    TQTimer m_timer;
};

#endif
