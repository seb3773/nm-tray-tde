#ifndef TEST_TRAY_H
#define TEST_TRAY_H

#include <ksystemtray.h>

class TQMouseEvent;

class TestTray : public KSystemTray
{
    TQ_OBJECT

public:
    TestTray();

protected:
    virtual void mousePressEvent(TQMouseEvent *e);
};

#endif
