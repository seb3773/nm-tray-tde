#include "traycontroller.h"
#include "nmtray.h"
#include "traypopup.h"
#include "hiddenwifidialog.h"
#include "createwifidialog.h"
#include "connectioneditordialog.h"
#include "connectioninfodialog.h"
#include "nm/nmeventpump.h"

#include <tdeapplication.h>
#include <tdelocale.h>
#include <tdemessagebox.h>
#include <tqcursor.h>
#include <tqevent.h>

TrayController::TrayController(TQObject *parent)
    : TQObject(parent)
    , m_data(&m_client)
    , m_tray(0)
    , m_mainPopup(0)
    , m_quickPopup(0)
    , m_mainMenuOpen(false)
    , m_lastWifiAvailableCount(0)
    , m_lastWifiMenuFingerprint(TQString::null)
    , m_wifiScanPollCount(0)
    , m_wifiToggling(false)
    , m_wifiToggleTarget(false)
    , m_wifiTogglePollCount(0)
    , m_networkingToggling(false)
    , m_networkingToggleTarget(false)
    , m_networkingTogglePollCount(0)
    , m_wiredToggling(false)
    , m_wiredToggleTarget(false)
    , m_connectAnimStage(0)
{
    connect(&m_wifiScanPollTimer, TQT_SIGNAL(timeout()),
            this, TQT_SLOT(onWifiScanPoll()));
    connect(&m_wifiToggleTimer, TQT_SIGNAL(timeout()),
            this, TQT_SLOT(onWifiTogglePoll()));
    connect(&m_networkingToggleTimer, TQT_SIGNAL(timeout()),
            this, TQT_SLOT(onNetworkingTogglePoll()));
    connect(&m_wiredToggleTimer, TQT_SIGNAL(timeout()),
            this, TQT_SLOT(onWiredTogglePoll()));
    connect(&m_connectAnimTimer, TQT_SIGNAL(timeout()),
            this, TQT_SLOT(onConnectAnimTick()));
    connect(&m_menuUpdateTimer, TQT_SIGNAL(timeout()),
            this, TQT_SLOT(onMainMenuUpdateTimeout()));
}

TrayController::~TrayController()
{
    delete m_mainPopup;
    delete m_quickPopup;
    delete m_tray;
}

bool TrayController::init(TQString *errorOut)
{
    if (!m_client.init(errorOut))
        return false;

    m_notifier.init();

    connect(&m_client, TQT_SIGNAL(changed()), this, TQT_SLOT(onClientChanged()));
    connect(&m_data, TQT_SIGNAL(refreshed()), this, TQT_SLOT(onDataRefreshed()));

    m_tray = new NmTray();
    connect(m_tray, TQT_SIGNAL(leftClicked(const TQPoint &)),
            this, TQT_SLOT(onTrayLeftClicked(const TQPoint &)));
    connect(m_tray, TQT_SIGNAL(rightClicked(const TQPoint &)),
            this, TQT_SLOT(onTrayRightClicked(const TQPoint &)));

    NmEventPump::pumpRepeated(10);
    m_data.refresh();
    updateTrayState();
    maybeStartBackgroundScan();
    startScanPollingIfNeeded();

    m_mainPopup = new NmTrayPopup(this, NmTrayPopup::MainPopup, 0);
    m_quickPopup = new NmTrayPopup(this, NmTrayPopup::QuickPopup, 0);
    connect(m_mainPopup, TQT_SIGNAL(popupClosed()),
            this, TQT_SLOT(onMainMenuClosed()));

    tqApp->installEventFilter(this);

    return true;
}

NmTray *TrayController::tray() const
{
    return m_tray;
}

void TrayController::registerMenuItem(int id, const NmItem &item)
{
    m_menuItems.insert(id, item);
}

void TrayController::clearMenuItems()
{
    m_menuItems.clear();
}

void TrayController::triggerMenuAction(int id)
{
    m_pendingActions.append(id);
    TQTimer::singleShot(0, this, SLOT(executePendingActions()));
}

void TrayController::executePendingActions()
{
    while (!m_pendingActions.isEmpty()) {
        int id = m_pendingActions.first();
        m_pendingActions.remove(m_pendingActions.begin());

        switch (id) {
        case IdToggleNetworking:
            onToggleNetworking();
            break;
        case IdToggleWireless:
            onToggleWireless();
            break;
        case IdToggleWired:
            onToggleWired();
            break;
        case IdToggleNotifications:
            onToggleNotifications();
            break;
        case IdEditConnections:
            onEditConnections();
            break;
        case IdConnectionInfo:
            onConnectionInfo();
            break;
        case IdRequestWifiScan:
            onRequestWifiScan();
            break;
        case IdQuit:
            onQuit();
            break;
        case IdDisconnectWifi:
            onDisconnectWifi();
            break;
        case IdDisconnectWired:
            onDisconnectWired();
            break;
        case IdHiddenWifi:
            onHiddenWifi();
            break;
        case IdCreateWifi:
            onCreateWifi();
            break;
        default:
            if (id >= IdWifiAvailableBase && id < IdWifiMoreBase) {
                onConnectWifiAvailable(id);
            } else if (id >= IdWifiMoreBase && id < IdWiredSavedBase) {
                onConnectWifiSavedOutOfRange(id);
            } else {
                onPopupItemActivated(id);
            }
            break;
        }
    }
}

bool TrayController::lookupMenuItem(int id, NmItem *out) const
{
    if (!out)
        return false;

    if (id >= IdWifiAvailableBase && id < IdWifiMoreBase) {
        int idx = id - IdWifiAvailableBase;
        const NmItemList &list = m_data.wifiAvailableItems();
        if (idx < 0 || idx >= (int) list.size())
            return false;
        *out = list[idx];
        return true;
    }

    if (id >= IdWifiMoreBase && id < IdWiredSavedBase) {
        int idx = id - IdWifiMoreBase;
        const NmItemList &list = m_data.wifiSavedOutOfRangeItems();
        if (idx < 0 || idx >= (int) list.size())
            return false;
        *out = list[idx];
        return true;
    }

    if (id >= IdWiredSavedBase && id < IdVpnBase) {
        int idx = id - IdWiredSavedBase;
        const NmItemList &list = m_data.wiredSavedItems();
        if (idx < 0 || idx >= (int) list.size())
            return false;
        *out = list[idx];
        return true;
    }

    if (id >= IdVpnBase && id < IdActiveWifi) {
        int idx = id - IdVpnBase;
        const NmItemList &list = m_data.vpnSavedItems();
        if (idx < 0 || idx >= (int) list.size())
            return false;
        *out = list[idx];
        return true;
    }

    if (id == IdActiveWired) {
        if (!m_data.hasActiveWired())
            return false;
        *out = m_data.activeWiredItem();
        return true;
    }

    if (id == IdActiveWifi) {
        if (!m_data.hasActiveWifi())
            return false;
        *out = m_data.activeWifiItem();
        return true;
    }

    if (!m_menuItems.contains(id))
        return false;

    *out = m_menuItems[id];
    return true;
}

void TrayController::onClientChanged()
{
    NmEventPump::pump();
    if (m_wifiToggling && m_client.wirelessEnabled() == m_wifiToggleTarget)
        finishWifiToggle();
    if (m_wiredToggling && m_data.wiredEnabled() == m_wiredToggleTarget)
        finishWiredToggle();
    if (m_networkingToggling
        && m_client.networkingEnabled() == m_networkingToggleTarget)
        finishNetworkingToggle();
    m_data.refresh();
}

void TrayController::onDataRefreshed()
{
    updateTrayState();
    m_notifier.updateFromData(m_data);
    refreshMainMenuIfOpen();
}

void TrayController::updateTrayState()
{
    if (!m_tray)
        return;

    TQString tip = m_data.trayTooltip();
    if (tip != m_lastTrayTooltip) {
        m_lastTrayTooltip = tip;
        m_tray->setStatusToolTip(tip);
    }

    if (m_data.isTrayConnecting()) {
        if (!m_connectAnimTimer.isActive()) {
            m_connectAnimStage = 0;
            m_tray->startConnectingAnimation();
            m_connectAnimTimer.start(120);
        }
        return;
    }

    if (m_connectAnimTimer.isActive())
        m_connectAnimTimer.stop();
    m_connectAnimStage = 0;
    m_tray->setStatusIcon(m_data.trayIconName());
}

void TrayController::onConnectAnimTick()
{
    if (!m_tray)
        return;

    if (!m_data.isTrayConnecting()) {
        updateTrayState();
        return;
    }

    m_connectAnimStage = (m_connectAnimStage + 1) % 7;
    m_tray->advanceConnectingFrame(m_connectAnimStage);
}

void TrayController::showMainMenu(const TQPoint &)
{
    TQPoint anchor = TQCursor::pos();
    m_mainMenuPos = anchor;
    m_mainMenuOpen = true;
    m_lastWifiAvailableCount = -1;
    m_lastWifiMenuFingerprint = TQString::null;
    m_menuItems.clear();

    NmEventPump::pumpRepeated(15);
    m_data.refresh();

    m_mainPopup->showNear(anchor);
    m_lastWifiMenuFingerprint = mainMenuFingerprint();
    m_lastWifiAvailableCount = (int) m_data.wifiAvailableItems().size();

    /* Scan only when stale/incomplete; user can force via "Wifi - request scan". */
    m_data.startWifiScanOnMenuOpen();
    startScanPollingIfNeeded();
}

void TrayController::showQuickMenu(const TQPoint &)
{
    m_menuItems.clear();
    startScanPollingIfNeeded();
    m_quickPopup->showNear(TQCursor::pos());
}

void TrayController::switchToMainMenu(const TQPoint &globalPos)
{
    if (m_quickPopup && m_quickPopup->isOpen())
        m_quickPopup->hide();

    if (m_mainPopup && m_mainPopup->isOpen()) {
        m_mainPopup->hide();
        onMainMenuClosed();
        return;
    }
    showMainMenu(globalPos);
}

void TrayController::switchToQuickMenu(const TQPoint &globalPos)
{
    if (m_mainPopup && m_mainPopup->isOpen()) {
        m_mainPopup->hide();
        onMainMenuClosed();
    }

    if (m_quickPopup && m_quickPopup->isOpen()) {
        m_quickPopup->hide();
        return;
    }
    showQuickMenu(globalPos);
}

bool TrayController::eventFilter(TQObject *watched, TQEvent *event)
{
    if (event->type() == TQEvent::MouseButtonPress
        && m_mainPopup && m_mainPopup->isOpen() && m_tray) {
        TQMouseEvent *mouseEvent = static_cast<TQMouseEvent *>(event);
        if (mouseEvent->button() == TQt::RightButton) {
            TQPoint trayOrigin = m_tray->mapToGlobal(TQPoint(0, 0));
            TQRect trayRect(trayOrigin, m_tray->size());
            if (trayRect.contains(mouseEvent->globalPos())) {
                switchToQuickMenu(mouseEvent->globalPos());
                return true;
            }
        }
    }

    return TQObject::eventFilter(watched, event);
}

void TrayController::onTrayLeftClicked(const TQPoint &globalPos)
{
    switchToMainMenu(globalPos);
}

void TrayController::onTrayRightClicked(const TQPoint &globalPos)
{
    switchToQuickMenu(globalPos);
}

void TrayController::onPopupItemActivated(int id)
{
    NmItem item;
    if (!lookupMenuItem(id, &item))
        return;

    if (item.isActive || item.type == NmItem::Active)
        m_data.deactivateItem(item);
    else
        m_data.activateItem(item);

    NmEventPump::pump();
    NmEventPump::pumpRepeated(10);
}

void TrayController::onToggleNetworking()
{
    if (m_networkingToggling)
        return;

    bool enable = !m_client.networkingEnabled();
    if (m_quickPopup && m_quickPopup->isOpen())
        enable = m_quickPopup->nextQuickToggleState(IdToggleNetworking);

    m_networkingToggling = true;
    m_networkingToggleTarget = enable;
    if (m_quickPopup && m_quickPopup->isOpen())
        m_quickPopup->setQuickToggleChecked(IdToggleNetworking, enable);

    m_client.setNetworkingEnabled(enable);

    m_networkingTogglePollCount = 0;
    if (!m_networkingToggleTimer.isActive())
        m_networkingToggleTimer.start(200);

    refreshMainMenuIfOpen();
}

void TrayController::onToggleWireless()
{
    if (m_wifiToggling || m_networkingToggling)
        return;

    bool enable = !m_client.wirelessEnabled();
    if (m_quickPopup && m_quickPopup->isOpen())
        enable = m_quickPopup->nextQuickToggleState(IdToggleWireless);

    m_wifiToggling = true;
    m_wifiToggleTarget = enable;
    if (m_quickPopup && m_quickPopup->isOpen())
        m_quickPopup->setQuickToggleChecked(IdToggleWireless, enable);

    m_client.setWirelessEnabled(enable);

    m_wifiTogglePollCount = 0;
    if (!m_wifiToggleTimer.isActive())
        m_wifiToggleTimer.start(200);

    refreshMainMenuIfOpen();
}

void TrayController::onToggleWired()
{
    if (m_wiredToggling || m_networkingToggling)
        return;

    bool enable = !m_data.wiredEnabled();
    if (m_quickPopup && m_quickPopup->isOpen())
        enable = m_quickPopup->nextQuickToggleState(IdToggleWired);

    m_wiredToggling = true;
    m_wiredToggleTarget = enable;
    if (m_quickPopup && m_quickPopup->isOpen())
        m_quickPopup->setQuickToggleChecked(IdToggleWired, enable);

    m_data.setWiredEnabled(enable);

    m_wiredTogglePollCount = 0;
    if (!m_wiredToggleTimer.isActive())
        m_wiredToggleTimer.start(200);

    refreshMainMenuIfOpen();
}

void TrayController::onToggleNotifications()
{
    m_notifier.setEnabled(!m_notifier.enabled());
}

void TrayController::onDisconnectWifi()
{
    NmEventPump::pump();
    m_data.deactivateWifi();
    NmEventPump::pumpRepeated(10);
    m_data.refresh();
    updateTrayState();
}

void TrayController::onDisconnectWired()
{
    NmEventPump::pump();
    m_data.deactivateWired();
    NmEventPump::pumpRepeated(10);
    m_data.refresh();
    updateTrayState();
}

void TrayController::onConnectWifiAvailable(int id)
{
    int idx = id - IdWifiAvailableBase;
    TQString error;

    if (!m_data.connectWifiAvailable(idx, &error)) {
        if (m_tray && !error.isEmpty())
            KMessageBox::error(m_tray, error, i18n("Wi-Fi Connection"));
        return;
    }

    NmEventPump::pump();
    NmEventPump::pumpRepeated(10);
    m_data.refresh();
    updateTrayState();
}

void TrayController::onConnectWifiSavedOutOfRange(int id)
{
    int idx = id - IdWifiMoreBase;
    TQString error;

    if (!m_data.connectWifiSavedOutOfRange(idx, &error)) {
        if (m_tray && !error.isEmpty())
            KMessageBox::error(m_tray, error, i18n("Wi-Fi Connection"));
        return;
    }

    NmEventPump::pump();
    NmEventPump::pumpRepeated(10);
    m_data.refresh();
    updateTrayState();
}

void TrayController::onEditConnections()
{
    ConnectionEditorDialog dlg(&m_client, m_tray);
    dlg.exec();
}

void TrayController::onConnectionInfo()
{
    NmEventPump::pump();
    ConnectionInfoDialog dlg(&m_client, m_tray);
    dlg.exec();
}

void TrayController::onRequestWifiScan()
{
    if (!m_client.networkingEnabled() || !m_client.wirelessEnabled())
        return;

    m_data.startWifiScanSession(true);
    m_data.requestWifiScan();

    if (m_quickPopup && m_quickPopup->isOpen())
        m_quickPopup->refreshQuickScanRow();

    m_wifiScanPollCount = 0;
    if (!m_wifiScanPollTimer.isActive())
        m_wifiScanPollTimer.start(200);
}

void TrayController::onHiddenWifi()
{
    NmEventPump::pump();
    m_data.refresh();
    HiddenWifiDialog dlg(&m_data, m_tray);
    dlg.exec();
}

void TrayController::onCreateWifi()
{
    NmEventPump::pump();
    m_data.refresh();

    if (!m_data.canCreateWifiHotspot()) {
        if (m_tray) {
            KMessageBox::sorry(m_tray,
                i18n("Creating a Wi-Fi hotspot is not available.\n"
                     "Check that Wi-Fi is enabled, the adapter supports "
                     "Access Point mode, and that you have permission "
                     "to share the connection."),
                i18n("Create New Wi-Fi Network"));
        }
        return;
    }

    CreateWifiDialog dlg(&m_data, m_tray);
    dlg.exec();
}

void TrayController::onMainMenuClosed()
{
    m_mainMenuOpen = false;
}

void TrayController::startScanPollingIfNeeded()
{
    if (!m_data.isWifiScanSessionActive())
        return;

    if (m_wifiScanPollTimer.isActive())
        return;

    m_wifiScanPollCount = 0;
    m_wifiScanPollTimer.start(200);
}

void TrayController::refreshMainMenuIfOpen()
{
    scheduleMainMenuUpdate();
}

TQString TrayController::mainMenuFingerprint() const
{
    TQString fingerprint = m_data.wifiMenuFingerprint();

    if (m_wifiToggling)
        fingerprint += m_wifiToggleTarget ? "|wifi-on" : "|wifi-off";
    if (m_networkingToggling)
        fingerprint += m_networkingToggleTarget ? "|net-on" : "|net-off";

    fingerprint += TQString("|net=%1|wifi=%2|wired=%3|vpn=%4")
        .arg(networkingEnabledForUi() ? 1 : 0)
        .arg(wirelessEnabledForUi() ? 1 : 0)
        .arg(m_data.hasActiveWired() ? m_data.activeWiredItem().name : TQString("-"))
        .arg((int) m_data.vpnSavedItems().size());

    return fingerprint;
}

void TrayController::scheduleMainMenuUpdate()
{
    if (!m_mainPopup || !m_mainPopup->isOpen())
        return;

    /* nm-applet: coalesce bursts into a single idle rebuild. */
    if (m_menuUpdateTimer.isActive())
        return;

    m_menuUpdateTimer.start(0, true);
}

void TrayController::onMainMenuUpdateTimeout()
{
    if (!m_mainPopup || !m_mainPopup->isOpen())
        return;

    /* Like nm-applet: defer while the saved-networks submenu is open. */
    if (m_mainPopup->isSavedSubmenuOpen()) {
        m_menuUpdateTimer.start(100, true);
        return;
    }

    TQString fingerprint = mainMenuFingerprint();
    if (fingerprint != m_lastWifiMenuFingerprint) {
        m_mainPopup->rebuild();
        m_lastWifiMenuFingerprint = fingerprint;
        m_lastWifiAvailableCount = (int) m_data.wifiAvailableItems().size();
        return;
    }

    /* Same network set: refresh signal levels in place (no rebuild / reshuffle). */
    m_mainPopup->refreshWifiSignalLevels();
}

void TrayController::onWifiScanPoll()
{
    /* Scan session watcher only: pump GLib so last-scan / AP signals arrive.
     * Menu/data refresh is event-driven via NmClient::changed (nm-applet style). */
    if (m_data.isWifiScanSessionActive())
        NmEventPump::pumpRepeated(5);
    else
        NmEventPump::pump();

    ++m_wifiScanPollCount;

    /* Retry RequestScan only while LastScan has not advanced yet (NOT_ALLOWED /
     * rate-limit). Never keep kicking new scans after the driver reported one. */
    if (m_data.isWifiScanSessionActive()
        && m_data.wifiScanAwaitingLastScan()
        && (m_wifiScanPollCount == 1 || (m_wifiScanPollCount % 10) == 0))
        m_data.requestWifiScan();

    if (m_quickPopup && m_quickPopup->isOpen())
        m_quickPopup->refreshQuickScanRow();

    if (m_data.isWifiScanSessionActive()
        && !m_data.wifiScanSessionComplete()
        && m_wifiScanPollCount < 80)
        return;

    if (!m_data.isWifiScanSessionActive()) {
        m_wifiScanPollTimer.stop();
        return;
    }

    m_wifiScanPollTimer.stop();
    NmEventPump::pumpRepeated(15);
    m_data.finishWifiScanSession();
    m_data.refresh();
    updateTrayState();
    scheduleMainMenuUpdate();
    if (m_quickPopup && m_quickPopup->isOpen())
        m_quickPopup->refreshQuickScanRow();
}

void TrayController::finishWifiToggle()
{
    m_wifiToggleTimer.stop();
    m_wifiToggling = false;
    m_wifiTogglePollCount = 0;

    NmEventPump::pumpRepeated(10);
    m_data.refresh();
    updateTrayState();

    if (m_quickPopup && m_quickPopup->isOpen()) {
        m_quickPopup->refreshQuickToggles();
        m_quickPopup->refreshQuickScanRow();
    }

    m_lastWifiMenuFingerprint = TQString::null;
    refreshMainMenuIfOpen();
}

void TrayController::onWifiTogglePoll()
{
    NmEventPump::pumpRepeated(5);
    ++m_wifiTogglePollCount;

    if (m_client.wirelessEnabled() == m_wifiToggleTarget) {
        finishWifiToggle();
        return;
    }

    if (m_wifiTogglePollCount >= 50) {
        finishWifiToggle();
        return;
    }

    refreshMainMenuIfOpen();
}

bool TrayController::networkingEnabledForUi() const
{
    if (m_networkingToggling)
        return m_networkingToggleTarget;
    return m_client.networkingEnabled();
}

bool TrayController::wirelessEnabledForUi() const
{
    if (m_wifiToggling)
        return m_wifiToggleTarget;
    return m_client.wirelessEnabled();
}

bool TrayController::wiredEnabledForUi() const
{
    if (m_wiredToggling)
        return m_wiredToggleTarget;
    return m_data.wiredEnabled();
}

bool TrayController::isQuickToggleBlocked(int actionId) const
{
    if (actionId == IdToggleNetworking && m_networkingToggling)
        return true;
    if (actionId == IdToggleWireless
        && (m_wifiToggling || m_networkingToggling))
        return true;
    if (actionId == IdToggleWired
        && (m_wiredToggling || m_networkingToggling))
        return true;
    return false;
}

void TrayController::finishWiredToggle()
{
    m_wiredToggleTimer.stop();
    m_wiredToggling = false;
    m_wiredTogglePollCount = 0;

    NmEventPump::pumpRepeated(10);
    m_data.refresh();
    updateTrayState();

    if (m_quickPopup && m_quickPopup->isOpen()) {
        m_quickPopup->refreshQuickToggles();
        m_quickPopup->refreshQuickScanRow();
    }

    m_lastWifiMenuFingerprint = TQString::null;
    refreshMainMenuIfOpen();
}

void TrayController::onWiredTogglePoll()
{
    NmEventPump::pumpRepeated(5);
    ++m_wiredTogglePollCount;

    if (m_data.wiredEnabled() == m_wiredToggleTarget) {
        finishWiredToggle();
        return;
    }

    if (m_wiredTogglePollCount >= 50) {
        finishWiredToggle();
        return;
    }

    refreshMainMenuIfOpen();
}

void TrayController::finishNetworkingToggle()
{
    m_networkingToggleTimer.stop();
    m_networkingToggling = false;
    m_networkingTogglePollCount = 0;

    NmEventPump::pumpRepeated(10);
    m_data.refresh();
    updateTrayState();

    if (m_quickPopup && m_quickPopup->isOpen()) {
        m_quickPopup->refreshQuickToggles();
        m_quickPopup->refreshQuickScanRow();
    }

    m_lastWifiMenuFingerprint = TQString::null;
    refreshMainMenuIfOpen();
}

void TrayController::onNetworkingTogglePoll()
{
    NmEventPump::pumpRepeated(5);
    ++m_networkingTogglePollCount;

    if (m_client.networkingEnabled() == m_networkingToggleTarget) {
        finishNetworkingToggle();
        return;
    }

    if (m_networkingTogglePollCount >= 50) {
        finishNetworkingToggle();
        return;
    }

    refreshMainMenuIfOpen();
}

void TrayController::maybeStartBackgroundScan()
{
    if (m_wifiScanPollTimer.isActive())
        return;

    if (!m_data.maybeStartBackgroundWifiScan())
        return;

    startScanPollingIfNeeded();
}

void TrayController::onQuit()
{
    tqApp->quit();
}

#include "traycontroller.moc"
