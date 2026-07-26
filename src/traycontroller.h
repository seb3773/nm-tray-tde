#ifndef TRAY_CONTROLLER_H
#define TRAY_CONTROLLER_H

#include <tqobject.h>
#include <tqmap.h>
#include <tqstring.h>
#include <tqtimer.h>

#include "nm/nmeventpump.h"
#include "nm/nmclient.h"
#include "nmdata.h"
#include "nmitem.h"
#include "nmnotifier.h"

class NmTray;
class NmTrayPopup;

class TrayController : public TQObject
{
    TQ_OBJECT

public:
    enum MenuActionId {
        IdToggleNetworking = 9000,
        IdToggleWireless = 9001,
        IdToggleWired = 9006,
        IdToggleNotifications = 9003,
        IdRequestWifiScan = 9005,
        IdConnectionInfo = 9004,
        IdEditConnections = 9002,
        IdQuit = 9999,
        IdDisconnectWifi = 8000,
        IdDisconnectWired = 8003,
        IdHiddenWifi = 8001,
        IdCreateWifi = 8002,
        IdWifiAvailableBase = 100,
        IdWifiMoreBase = 500,
        IdWiredSavedBase = 1000,
        IdVpnBase = 1500,
        IdActiveWifi = 2000,
        IdActiveWired = 2001
    };

    explicit TrayController(TQObject *parent = 0);
    ~TrayController();

    bool init(TQString *errorOut = 0);
    NmTray *tray() const;

    NmClient *client() { return &m_client; }
    NmData *data() { return &m_data; }
    NmNotifier *notifier() { return &m_notifier; }
    void registerMenuItem(int id, const NmItem &item);
    void clearMenuItems();
    void triggerMenuAction(int id);
    void switchToQuickMenu(const TQPoint &globalPos);
    void switchToMainMenu(const TQPoint &globalPos);

    bool isWifiToggling() const { return m_wifiToggling; }
    bool wifiToggleTarget() const { return m_wifiToggleTarget; }
    bool isNetworkingToggling() const { return m_networkingToggling; }
    bool networkingToggleTarget() const { return m_networkingToggleTarget; }
    bool isWiredToggling() const { return m_wiredToggling; }
    bool wiredToggleTarget() const { return m_wiredToggleTarget; }
    bool networkingEnabledForUi() const;
    bool wirelessEnabledForUi() const;
    bool wiredEnabledForUi() const;
    bool isQuickToggleBlocked(int actionId) const;

    bool eventFilter(TQObject *watched, TQEvent *event);

private slots:
    void onClientChanged();
    void onDataRefreshed();
    void onTrayLeftClicked(const TQPoint &globalPos);
    void onTrayRightClicked(const TQPoint &globalPos);
    void onPopupItemActivated(int id);
    void onToggleNetworking();
    void onToggleWireless();
    void onToggleWired();
    void onToggleNotifications();
    void onDisconnectWifi();
    void onDisconnectWired();
    void onConnectWifiAvailable(int id);
    void onConnectWifiSavedOutOfRange(int id);
    void onEditConnections();
    void onConnectionInfo();
    void onRequestWifiScan();
    void onHiddenWifi();
    void onCreateWifi();
    void onQuit();
    void onMainMenuClosed();
    void onWifiScanPoll();
    void onWifiTogglePoll();
    void onWiredTogglePoll();
    void onNetworkingTogglePoll();
    void onConnectAnimTick();
    void onMainMenuUpdateTimeout();
    void executePendingActions();

private:
    void finishWifiToggle();
    void finishWiredToggle();
    void finishNetworkingToggle();
    void updateTrayState();
    void scheduleMainMenuUpdate();
    void refreshMainMenuIfOpen();
    TQString mainMenuFingerprint() const;
    void maybeStartBackgroundScan();
    void startScanPollingIfNeeded();
    void showMainMenu(const TQPoint &globalPos);
    void showQuickMenu(const TQPoint &globalPos);
    bool lookupMenuItem(int id, NmItem *out) const;

    NmEventPump m_pump;
    NmClient m_client;
    NmData m_data;
    NmNotifier m_notifier;
    NmTray *m_tray;
    NmTrayPopup *m_mainPopup;
    NmTrayPopup *m_quickPopup;
    TQValueList<int> m_pendingActions;
    TQMap<int, NmItem> m_menuItems;
    TQPoint m_mainMenuPos;
    bool m_mainMenuOpen;
    int m_lastWifiAvailableCount;
    TQString m_lastWifiMenuFingerprint;
    TQTimer m_wifiScanPollTimer;
    int m_wifiScanPollCount;
    TQTimer m_menuUpdateTimer;
    bool m_wifiToggling;
    bool m_wifiToggleTarget;
    TQTimer m_wifiToggleTimer;
    int m_wifiTogglePollCount;
    bool m_networkingToggling;
    bool m_networkingToggleTarget;
    TQTimer m_networkingToggleTimer;
    int m_networkingTogglePollCount;
    bool m_wiredToggling;
    bool m_wiredToggleTarget;
    TQTimer m_wiredToggleTimer;
    int m_wiredTogglePollCount;
    TQTimer m_connectAnimTimer;
    int m_connectAnimStage;
    TQString m_lastTrayTooltip;
};

#endif
