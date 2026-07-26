#ifndef NM_PROBE_H
#define NM_PROBE_H

#include <tqobject.h>

class NmClient;

class ProbeWatcher : public TQObject
{
    TQ_OBJECT

public:
    ProbeWatcher(NmClient *client, TQObject *parent = 0);

public slots:
    void refresh();

private:
    NmClient *m_client;
};

#endif
