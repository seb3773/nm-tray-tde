#ifndef TRAY_APP_H
#define TRAY_APP_H

#include <kuniqueapplication.h>

class TrayController;

class TrayApp : public KUniqueApplication
{
    TQ_OBJECT

public:
    TrayApp();
    ~TrayApp();

private:
    TrayController *m_controller;
};

#endif
