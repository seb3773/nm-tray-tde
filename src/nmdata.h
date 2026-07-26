#ifndef NM_DATA_H
#define NM_DATA_H

#include <tqobject.h>
#include <tqstring.h>
#include <tqmap.h>

#include "nmitem.h"
#include "wifisecurity.h"

class NmClient;

class NmData : public TQObject
{
    TQ_OBJECT

public:
    explicit NmData(NmClient *client, TQObject *parent = 0);

    void refresh();

    const NmItemList &activeItems() const { return m_active; }
    const NmItemList &wifiAvailableItems() const { return m_wifiAvailable; }
    const NmItemList &wifiSavedOutOfRangeItems() const { return m_wifiSavedOutOfRange; }
    const NmItemList &wiredSavedItems() const { return m_wiredSaved; }
    const NmItemList &vpnSavedItems() const { return m_vpnSaved; }

    bool hasActiveWifi() const { return m_hasActiveWifi; }
    NmItem activeWifiItem() const { return m_activeWifiItem; }
    bool hasActiveWired() const { return m_hasActiveWired; }
    NmItem activeWiredItem() const { return m_activeWiredItem; }
    bool hasActiveVpn() const { return m_hasActiveVpn; }

    TQString primaryConnectionName() const { return m_primaryConnectionName; }
    TQString primaryConnectionTypeLabel() const { return m_primaryConnectionTypeLabel; }
    TQString primaryConnectionIconName() const;

    TQString trayIconName() const;
    TQString trayTooltip() const;
    bool isTrayConnecting() const;
    bool shouldNotifyConnectionLost() const;

    TQString primaryWifiDevicePath() const { return m_primaryWifiDevice; }
    TQString primaryWifiIface() const { return m_primaryWifiIface; }
    bool hasWifiDevice() const { return m_hasWifiDevice; }
    bool hasWiredDevice() const { return m_hasWiredDevice; }
    bool isWiredCablePlugged() const { return m_isWiredCablePlugged; }
    TQString primaryWiredIface() const { return m_primaryWiredIface; }

    bool wiredEnabled() const;
    void setWiredEnabled(bool enabled);

    TQString wifiMenuFingerprint() const;

    /* Wi-Fi scan (nm-applet style: async, all devices, persistent cache). */
    void requestWifiScan();
    bool maybeStartBackgroundWifiScan();
    void startWifiScanOnMenuOpen();
    void startWifiScanSession(bool userRequested);
    void finishWifiScanSession();
    bool isWifiScanSessionActive() const { return m_wifiScanActive; }
    bool isWifiScanUserRequested() const { return m_wifiScanUserRequested; }
    bool isWifiScanning() const { return m_wifiScanUserRequested && m_wifiScanActive; }
    bool wifiScanSessionComplete() const;
    bool wifiScanAwaitingLastScan() const;

public slots:
    void activateItem(const NmItem &item);
    void deactivateItem(const NmItem &item);
    void deactivateWifi();
    void deactivateWired();
    bool connectHiddenWifi(const TQString &ssid, WifiSecurityType security,
                           const TQString &identity, const TQString &secret,
                           TQString *errorOut = 0);
    bool createWifiHotspot(const TQString &ssid, WifiSecurityType security,
                           const TQString &password, TQString *errorOut = 0);
    bool connectWifiAvailable(int index, TQString *errorOut = 0);
    bool connectWifiSavedOutOfRange(int index, TQString *errorOut = 0);
    bool isHiddenWifiSecurityAvailable(WifiSecurityType type) const;
    bool canCreateWifiHotspot() const;
    bool isCreateWifiSecurityAvailable(WifiSecurityType type) const;

signals:
    void refreshed();

private:
    void rebuildActive();
    void rebuildSaved();
    void rebuildWifi();
    void updatePrimaryConnection();

    NmClient *m_client;
    NmItemList m_active;
    NmItemList m_wifiAvailable;
    NmItemList m_wifiSavedOutOfRange;
    NmItemList m_wiredSaved;
    NmItemList m_vpnSaved;
    NmItem m_activeWifiItem;
    NmItem m_activatingWifiItem;
    NmItem m_activeWiredItem;
    bool m_hasActiveWifi;
    bool m_hasActivatingWifi;
    bool m_hasActiveWired;
    bool m_hasActiveVpn;
    bool m_hasWifiDevice;
    bool m_hasWiredDevice;
    bool m_isWiredCablePlugged;
    TQString m_primaryWifiDevice;
    TQString m_primaryWifiIface;
    TQString m_primaryWiredIface;
    TQString m_activeWifiUuid;
    TQString m_primaryConnectionUuid;
    TQString m_primaryConnectionName;
    TQString m_primaryConnectionTypeLabel;
    TQString m_primaryConnectionCtype;
    bool m_wifiScanActive;
    bool m_wifiScanUserRequested;
    long long m_wifiScanStartedMs;
    TQMap<TQString, NmItem> m_wifiNetworkCache;
    TQMap<TQString, int> m_wifiCacheMisses;
};

#endif
