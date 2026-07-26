#ifndef NM_NOTIFIER_H
#define NM_NOTIFIER_H

#include <tqobject.h>
#include <tqstring.h>

class NmData;

class NmNotifier : public TQObject
{
    TQ_OBJECT

public:
    NmNotifier();
    ~NmNotifier();

    bool init();
    void shutdown();

    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);

    void updateFromData(const NmData &data);

private:
    void showNotification(const TQString &summary, const TQString &body,
                          const TQString &iconName);

    bool m_initialized;
    bool m_enabled;
    bool m_seeded;
    bool m_hadConnection;
    TQString m_lastConnectionName;
    TQString m_lastConnectionTypeLabel;
};

#endif
