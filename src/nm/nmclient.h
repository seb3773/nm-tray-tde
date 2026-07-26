#ifndef NM_CLIENT_H
#define NM_CLIENT_H

#include <tqobject.h>
#include <tqstring.h>

typedef struct _NMClient NMClient;
typedef struct _NMConnection NMConnection;
typedef struct _NMDevice NMDevice;

class NmClient : public TQObject
{
    TQ_OBJECT

public:
    explicit NmClient(TQObject *parent = 0);
    ~NmClient();

    bool init(TQString *errorOut = 0, bool enableSecretAgent = true);
    void shutdown();

    NMClient *nmClient() const { return m_client; }

    bool isNmRunning() const;
    int clientState() const;
    bool networkingEnabled() const;
    bool wirelessEnabled() const;
    bool wiredEnabled() const;
    bool wirelessHardwareEnabled() const;
    int connectivity() const;

    bool setNetworkingEnabled(bool enabled);
    bool setWirelessEnabled(bool enabled);
    bool setWiredEnabled(bool enabled);

    bool activateConnection(const TQString &connectionPath,
                            const TQString &devicePath,
                            const TQString &specificObject = TQString::null);
    bool activateConnection(NMConnection *connection, NMDevice *device,
                            const TQString &specificObject = TQString::null);
    bool deactivateDevice(const TQString &devicePath);
    bool deactivateActiveConnection(const TQString &uuidOrConnectionPath);
    bool deactivateWifiConnections();
    bool deactivateWifiOnDevice(NMDevice *device);
    void abortActivatingConnections(bool isWifi);
    bool requestWifiScan(const TQString &devicePath);
    bool requestWifiScanAndWait(const TQString &devicePath, int timeoutMs);

    void dumpStatus() const;
    void notifyChanged();
    void connectDevice(void *device);
    void disconnectDevice(void *device);

signals:
    void changed();

private:
    void connectClientSignals();
    void emitChanged();

    NMClient *m_client;
    void *m_secretAgent;
    void *m_deviceSignals; /* opaque: TQMap<void*, DeviceSignals>* */
};

#endif
