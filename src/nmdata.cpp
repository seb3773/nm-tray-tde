#include "nmdata.h"
#include "nm/nmclient.h"
#include "nm/nmeventpump.h"
#include "nm/glib_compat.h"
#include "icons.h"
#include "wifisecurity.h"
#include "wifiutil.h"

#include <tdelocale.h>

#include <tqmap.h>
#include <tqstringlist.h>

namespace {

struct SavedWifiProfile {
    TQString name;
    TQString uuid;
    TQString path;
    TQString ssid;
};



bool isVirtualIfaceName(const char *iface)
{
    if (!iface)
        return false;

    if (strcmp(iface, "lo") == 0)
        return true;
    return isSystemVirtualIfaceName(iface);
}

bool isEthernetSavedConnection(NMRemoteConnection *conn)
{
    NMSettingConnection *s_con;
    const char *ctype;
    const char *id;

    if (!conn)
        return false;

    s_con = nm_connection_get_setting_connection(NM_CONNECTION(conn));
    if (!s_con)
        return false;

    ctype = nm_setting_connection_get_connection_type(s_con);
    if (!ctype || strcmp(ctype, NM_SETTING_WIRED_SETTING_NAME) != 0)
        return false;

    id = nm_setting_connection_get_id(s_con);
    if (id && isVirtualIfaceName(id))
        return false;

    return true;
}

bool savedProfileListedInAvailable(const SavedWifiProfile &profile,
                                   const NmItemList &available)
{
    for (uint i = 0; i < available.size(); ++i) {
        if (ssidsMatch(available[i].name, profile.ssid))
            return true;
    }
    return false;
}

SavedWifiProfile *findSavedProfileForSsid(TQValueList<SavedWifiProfile> &profiles,
                                          const TQString &ssid)
{
    for (TQValueList<SavedWifiProfile>::Iterator it = profiles.begin();
         it != profiles.end(); ++it) {
        if (ssidsMatch(ssid, (*it).ssid))
            return &(*it);
    }
    return 0;
}

bool activateUnknownWifiNetwork(NmClient *clientWrapper, NMClient *client,
                                NMDevice *device, const NmItem &item,
                                const char *apPath)
{
    NMConnection *newConn;
    NMSettingConnection *s_con;
    NMSettingWireless *s_wifi;
    NMSettingWirelessSecurity *s_wsec;
    GBytes *ssid;

    if (!clientWrapper || !client || !device || !apPath)
        return false;

    newConn = nm_simple_connection_new();
    s_con = NM_SETTING_CONNECTION(nm_setting_connection_new());
    s_wifi = NM_SETTING_WIRELESS(nm_setting_wireless_new());

    TQCString itemNameUtf8 = item.name.utf8();
    g_object_set(G_OBJECT(s_con),
                 NM_SETTING_CONNECTION_ID, itemNameUtf8.data(),
                 NM_SETTING_CONNECTION_TYPE, NM_SETTING_WIRELESS_SETTING_NAME,
                 NM_SETTING_CONNECTION_AUTOCONNECT, FALSE,
                 NULL);
    ssid = g_bytes_new(itemNameUtf8.data(), itemNameUtf8.length());
    g_object_set(G_OBJECT(s_wifi),
                 NM_SETTING_WIRELESS_SSID, ssid,
                 NM_SETTING_WIRELESS_MODE, NM_SETTING_WIRELESS_MODE_INFRA,
                 NULL);
    g_bytes_unref(ssid);
    if (item.secured) {
        s_wsec = NM_SETTING_WIRELESS_SECURITY(nm_setting_wireless_security_new());
        g_object_set(G_OBJECT(s_wsec),
                     NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "wpa-psk",
                     NULL);
        nm_connection_add_setting(newConn, NM_SETTING(s_wsec));
    }
    nm_connection_add_setting(newConn, NM_SETTING(s_con));
    nm_connection_add_setting(newConn, NM_SETTING(s_wifi));

    nm_client_add_and_activate_connection_async(
        client, newConn, device, apPath, NULL, NULL, NULL);
    g_object_unref(newConn);
    NmEventPump::pumpAfterAsync();
    return true;
}

bool activateWifiMenuItem(NmClient *clientWrapper, NmItem item,
                          TQString *errorOut = 0)
{
    NMClient *client;
    NMDeviceWifi *wifi;
    NMDevice *device;
    NMRemoteConnection *conn;
    NMAccessPoint *ap;
    const char *apPathCStr;

    NmEventPump::pump();

    if (!clientWrapper) {
        if (errorOut)
            *errorOut = i18n("NetworkManager is not available.");
        return false;
    }

    client = clientWrapper->nmClient();
    if (!client || !clientWrapper->wirelessEnabled()) {
        if (errorOut)
            *errorOut = i18n("Wi-Fi is disabled.");
        return false;
    }

    wifi = pickPrimaryWifiDevice(client);
    if (!wifi) {
        if (errorOut)
            *errorOut = i18n("No Wi-Fi device available.");
        return false;
    }

    device = NM_DEVICE(wifi);

    /* Explicit switch: drop ACTIVATED/ACTIVATING Wi-Fi on this device before
     * starting the new association (clearer than relying on NM alone). */
    clientWrapper->deactivateWifiOnDevice(device);
    NmEventPump::pumpRepeated(5);

    conn = 0;
    if (!item.path.isEmpty()) {
        conn = nm_client_get_connection_by_path(
            client, item.path.utf8().data());
    }

    TQString ssidForLookup = item.name;
    if (conn) {
        NMSettingWireless *s_wifi = nm_connection_get_setting_wireless(
            NM_CONNECTION(conn));
        if (s_wifi) {
            TQString connSsid = ssidToString(nm_setting_wireless_get_ssid(s_wifi));
            if (!connSsid.isEmpty())
                ssidForLookup = connSsid;
        }
    }

    ap = 0;
    if (!item.specificObject.isEmpty()) {
        ap = nm_device_wifi_get_access_point_by_path(
            wifi, item.specificObject.utf8().data());
    }
    if (!ap)
        ap = accessPointForSsid(wifi, ssidForLookup);

    apPathCStr = "/";
    if (ap)
        apPathCStr = nm_object_get_path(NM_OBJECT(ap));

    if (!conn)
        conn = savedConnectionForSsidOnDevice(device, ssidForLookup);
    if (!conn && ap)
        conn = savedConnectionForAccessPoint(client, device, ap);

    if (conn) {
        TQString specific;
        if (ap)
            specific = TQString::fromUtf8(apPathCStr);
        if (!clientWrapper->activateConnection(NM_CONNECTION(conn), device,
                                               specific)) {
            if (errorOut)
                *errorOut = i18n("Could not activate the saved connection.");
            return false;
        }
        return true;
    }

    if (ap && strcmp(apPathCStr, "/") != 0)
        return activateUnknownWifiNetwork(clientWrapper, client, device, item,
                                          apPathCStr);

    if (errorOut)
        *errorOut = i18n("Could not find a saved connection for this network.");
    return false;
}

bool isSavedConnectionVisible(NMRemoteConnection *conn)
{
    NMSettingConnection *s_con;
    const char *ctype;
    const char *iface;
    const char *id;

    if (!conn)
        return false;

    s_con = nm_connection_get_setting_connection(NM_CONNECTION(conn));
    if (!s_con)
        return false;

    if (nm_setting_connection_get_master(s_con))
        return false;

    ctype = nm_setting_connection_get_connection_type(s_con);
    if (!ctype)
        return false;

    if (strcmp(ctype, NM_SETTING_VLAN_SETTING_NAME) == 0)
        return false;
    if (strcmp(ctype, NM_SETTING_BRIDGE_PORT_SETTING_NAME) == 0)
        return false;
    if (strcmp(ctype, NM_SETTING_BOND_PORT_SETTING_NAME) == 0)
        return false;
    if (strcmp(ctype, NM_SETTING_TEAM_PORT_SETTING_NAME) == 0)
        return false;
    if (strcmp(ctype, NM_SETTING_BRIDGE_SETTING_NAME) == 0)
        return false;
    if (strcmp(ctype, NM_SETTING_BLUETOOTH_SETTING_NAME) == 0)
        return false;

    iface = nm_setting_connection_get_interface_name(s_con);
    id = nm_setting_connection_get_id(s_con);
    if (isSystemVirtualIfaceName(iface) || isSystemVirtualIfaceName(id))
        return false;

    return true;
}

NMActiveConnection *bestActivatingConnection(NMClient *client, NMDevice **outDevice)
{
    NMActiveConnection *best = 0;
    NMDevice *bestDev = 0;

    if (outDevice)
        *outDevice = 0;
    if (!client)
        return 0;

    const GPtrArray *connections = nm_client_get_active_connections(client);
    for (guint i = 0; connections && i < connections->len; ++i) {
        NMActiveConnection *candidate = (NMActiveConnection *) g_ptr_array_index(connections, i);
        const GPtrArray *devices;
        NMDevice *candidateDev;

        if (nm_active_connection_get_state(candidate) != NM_ACTIVE_CONNECTION_STATE_ACTIVATING)
            continue;

        devices = nm_active_connection_get_devices(candidate);
        if (!devices || !devices->len)
            continue;

        candidateDev = (NMDevice *) g_ptr_array_index(devices, 0);
        if (!isRelevantDevice(candidateDev))
            continue;

        if (!bestDev) {
            bestDev = candidateDev;
            best = candidate;
            continue;
        }

        if (NM_IS_DEVICE_WIFI(bestDev) && NM_IS_DEVICE_ETHERNET(candidateDev)) {
            bestDev = candidateDev;
            best = candidate;
        }
    }

    if (outDevice)
        *outDevice = bestDev;
    return best;
}

bool anyActivatingConnection(NMClient *client)
{
    if (!client)
        return false;

    const GPtrArray *connections = nm_client_get_active_connections(client);
    for (guint i = 0; connections && i < connections->len; ++i) {
        NMActiveConnection *candidate = (NMActiveConnection *) g_ptr_array_index(connections, i);
        if (nm_active_connection_get_state(candidate) == NM_ACTIVE_CONNECTION_STATE_ACTIVATING) {
            const GPtrArray *devices = nm_active_connection_get_devices(candidate);
            bool hasRelevant = false;
            if (devices && devices->len > 0) {
                for (guint j = 0; j < devices->len; ++j) {
                    if (isRelevantDevice(NM_DEVICE(g_ptr_array_index(devices, j)))) {
                        hasRelevant = true;
                        break;
                    }
                }
            } else if (nm_active_connection_get_vpn(candidate)) {
                hasRelevant = true;
            }
            if (hasRelevant)
                return true;
        }
    }
    return false;
}

bool anyDeviceConnecting(NMClient *client)
{
    if (!client)
        return false;

    const GPtrArray *devices = nm_client_get_devices(client);
    for (guint i = 0; devices && i < devices->len; ++i) {
        NMDevice *device = (NMDevice *) g_ptr_array_index(devices, i);
        NMDeviceState state;

        if (!isRelevantDevice(device))
            continue;

        state = nm_device_get_state(device);
        if (state >= NM_DEVICE_STATE_PREPARE && state < NM_DEVICE_STATE_ACTIVATED)
            return true;
    }
    return false;
}

NMActiveConnection *defaultActiveConnection(NMClient *client, NMDevice **outDevice)
{
    NMActiveConnection *fallback = 0;
    NMDevice *fallbackDev = 0;

    if (outDevice)
        *outDevice = 0;
    if (!client)
        return 0;

    const GPtrArray *connections = nm_client_get_active_connections(client);
    for (guint i = 0; connections && i < connections->len; ++i) {
        NMActiveConnection *candidate = (NMActiveConnection *) g_ptr_array_index(connections, i);
        const GPtrArray *devices;
        NMDevice *candidateDev;

        devices = nm_active_connection_get_devices(candidate);
        if (!devices || !devices->len)
            continue;

        candidateDev = (NMDevice *) g_ptr_array_index(devices, 0);

        if (nm_active_connection_get_default(candidate)) {
            if (outDevice)
                *outDevice = candidateDev;
            return candidate;
        }

        if (!fallback && isRelevantDevice(candidateDev)) {
            fallback = candidate;
            fallbackDev = candidateDev;
        }
    }

    if (outDevice)
        *outDevice = fallbackDev;
    return fallback;
}

static NMDevice *deviceForActiveConnection(NMActiveConnection *active)
{
    const GPtrArray *devices;

    if (!active)
        return 0;

    devices = nm_active_connection_get_devices(active);
    if (!devices || !devices->len)
        return 0;

    return (NMDevice *) g_ptr_array_index(devices, 0);
}

NMActiveConnection *resolvePrimaryActiveConnection(NMClient *client, NMDevice **outDevice)
{
    NMActiveConnection *active = 0;
    NMDevice *device = 0;

    if (outDevice)
        *outDevice = 0;
    if (!client)
        return 0;

    active = bestActivatingConnection(client, &device);
    if (active) {
        if (outDevice)
            *outDevice = device;
        return active;
    }

    active = nm_client_get_primary_connection(client);
    if (active) {
        device = deviceForActiveConnection(active);
        if (outDevice)
            *outDevice = device;
        return active;
    }

    active = defaultActiveConnection(client, &device);
    if (outDevice)
        *outDevice = device;
    return active;
}

NMDevice *bestTrayDevice(NMClient *client)
{
    NMDevice *device = 0;
    resolvePrimaryActiveConnection(client, &device);
    return device;
}

TQString stateLabel(NMDeviceState state)
{
    switch (state) {
    case NM_DEVICE_STATE_UNKNOWN:
        return i18n("Unknown");
    case NM_DEVICE_STATE_UNMANAGED:
        return i18n("Unmanaged");
    case NM_DEVICE_STATE_UNAVAILABLE:
        return i18n("Down");
    case NM_DEVICE_STATE_DISCONNECTED:
        return i18n("Disconnected");
    case NM_DEVICE_STATE_PREPARE:
        return i18n("Preparing");
    case NM_DEVICE_STATE_CONFIG:
        return i18n("Configuration");
    case NM_DEVICE_STATE_NEED_AUTH:
        return i18n("Awaiting authentication");
    case NM_DEVICE_STATE_IP_CONFIG:
    case NM_DEVICE_STATE_IP_CHECK:
        return i18n("IP configuration");
    case NM_DEVICE_STATE_SECONDARIES:
        return i18n("Secondary connections");
    case NM_DEVICE_STATE_ACTIVATED:
        return i18n("Activated");
    case NM_DEVICE_STATE_DEACTIVATING:
        return i18n("Deactivating");
    case NM_DEVICE_STATE_FAILED:
        return i18n("Failed");
    default:
        return i18n("Unknown");
    }
}

TQString iconForDevice(NMDevice *device)
{
    if (!device)
        return NmIcons::disabledTrayIcon();

    NMDeviceState state = nm_device_get_state(device);
    bool isWifi = NM_IS_DEVICE_WIFI(device);

    if (state == NM_DEVICE_STATE_ACTIVATED) {
        if (isWifi) {
            NMAccessPoint *ap = nm_device_wifi_get_active_access_point(NM_DEVICE_WIFI(device));
            if (ap)
                return NmIcons::signalStrengthIcon(nm_access_point_get_strength(ap));
            return NmIcons::signalStrengthIcon(0);
        }
        if (NM_IS_DEVICE_ETHERNET(device))
            return NmIcons::wiredIcon();
    }

    if (state >= NM_DEVICE_STATE_PREPARE && state < NM_DEVICE_STATE_ACTIVATED)
        return NmIcons::deviceStateIcon((int) state);
    if (state == NM_DEVICE_STATE_DEACTIVATING)
        return NmIcons::deviceStateIcon((int) state);

    if (state == NM_DEVICE_STATE_DISCONNECTED) {
        if (isWifi)
            return NmIcons::notConnectedIcon();
        return NmIcons::notConnectedIcon();
    }

    if (isWifi)
        return NmIcons::wirelessOffIcon();

    return NmIcons::offlineIcon();
}

TQString tooltipForDevice(NMDevice *device, NMActiveConnection *active)
{
    TQStringList lines;

    if (!device)
        return i18n("No network connection");

    lines << i18n("Device: %1").arg(TQString::fromUtf8(nm_device_get_iface(device)));
    lines << i18n("State: %1").arg(stateLabel(nm_device_get_state(device)));

    if (active && NM_IS_DEVICE_WIFI(device)) {
        NMAccessPoint *ap = nm_device_wifi_get_active_access_point(NM_DEVICE_WIFI(device));
        if (ap) {
            TQString ssid = ssidToString(nm_access_point_get_ssid(ap));
            if (!ssid.isEmpty())
                lines << i18n("Network: %1").arg(ssid);
            lines << i18n("Signal Strength: %1%").arg(nm_access_point_get_strength(ap));
        } else {
            const char *id = nm_active_connection_get_id(active);
            if (id)
                lines << i18n("Network: %1").arg(TQString::fromUtf8(id));
        }
    } else if (active) {
        const char *id = nm_active_connection_get_id(active);
        if (id)
            lines << i18n("Network: %1").arg(TQString::fromUtf8(id));
    }

    return lines.join("\n");
}

TQValueList<SavedWifiProfile> loadSavedWifiProfiles(NMClient *client)
{
    TQValueList<SavedWifiProfile> profiles;
    const GPtrArray *connections = nm_client_get_connections(client);

    for (guint i = 0; connections && i < connections->len; ++i) {
        NMRemoteConnection *conn = (NMRemoteConnection *) g_ptr_array_index(connections, i);
        NMSettingConnection *s_con;
        NMSettingWireless *s_wifi;
        SavedWifiProfile profile;

        if (!isSavedConnectionVisible(conn))
            continue;

        s_con = nm_connection_get_setting_connection(NM_CONNECTION(conn));
        s_wifi = nm_connection_get_setting_wireless(NM_CONNECTION(conn));
        if (!s_con || !s_wifi)
            continue;

        profile.name = TQString::fromUtf8(nm_setting_connection_get_id(s_con));
        profile.uuid = TQString::fromUtf8(nm_setting_connection_get_uuid(s_con));
        profile.path = TQString::fromUtf8(nm_connection_get_path(NM_CONNECTION(conn)));
        profile.ssid = ssidToString(nm_setting_wireless_get_ssid(s_wifi));
        profiles.append(profile);
    }

    return profiles;
}

} // namespace

static gint64 wifiDeviceLastScanMs(NMDeviceWifi *wifi)
{
    gint64 lastScan = nm_device_wifi_get_last_scan(wifi);

    if (lastScan == -1)
        return G_MININT64;
    return lastScan;
}

/* Auto-scan only when the AP list is essentially empty. Freshness refreshes
 * are left to the explicit "Wifi - request scan" action. */
static bool primaryWifiListSparse(NMClient *client)
{
    NMDeviceWifi *wifi;
    const GPtrArray *aps;

    if (!client)
        return false;

    wifi = pickPrimaryWifiDevice(client);
    if (!wifi)
        return false;

    aps = nm_device_wifi_get_access_points(wifi);
    return !aps || aps->len <= 1;
}

NmData::NmData(NmClient *client, TQObject *parent)
    : TQObject(parent)
    , m_client(client)
    , m_hasActiveWifi(false)
    , m_hasActivatingWifi(false)
    , m_hasActiveWired(false)
    , m_hasActiveVpn(false)
    , m_hasWifiDevice(false)
    , m_wifiScanActive(false)
    , m_wifiScanUserRequested(false)
    , m_wifiScanStartedMs(0)
{
}

void NmData::rebuildActive()
{
    m_active.clear();
    m_activeWifiUuid = TQString::null;
    m_hasActiveWifi = false;
    m_hasActivatingWifi = false;
    m_hasActiveWired = false;
    m_activeWifiItem = NmItem();
    m_activatingWifiItem = NmItem();
    m_activeWiredItem = NmItem();

    NMClient *client = m_client ? m_client->nmClient() : 0;
    if (!client)
        return;

    const GPtrArray *connections = nm_client_get_active_connections(client);
    for (guint i = 0; connections && i < connections->len; ++i) {
        NMActiveConnection *ac = (NMActiveConnection *) g_ptr_array_index(connections, i);
        const GPtrArray *devices;
        NMDevice *device = 0;
        NMSettingConnection *s_con;
        const char *ctype;
        NmItem item;

        NMActiveConnectionState state = nm_active_connection_get_state(ac);
        if (state != NM_ACTIVE_CONNECTION_STATE_ACTIVATED && state != NM_ACTIVE_CONNECTION_STATE_ACTIVATING)
            continue;

        devices = nm_active_connection_get_devices(ac);
        if (devices && devices->len)
            device = (NMDevice *) g_ptr_array_index(devices, 0);

        if (!isMenuVisibleDevice(device))
            continue;

        item.type = NmItem::Active;
        item.name = TQString::fromUtf8(nm_active_connection_get_id(ac));
        item.uuid = TQString::fromUtf8(nm_active_connection_get_uuid(ac));
        item.path = TQString::fromUtf8(
            nm_connection_get_path(NM_CONNECTION(nm_active_connection_get_connection(ac))));
        if (device)
            item.devicePath = TQString::fromUtf8(
                nm_object_get_path(NM_OBJECT(device)));
        item.isActive = (state == NM_ACTIVE_CONNECTION_STATE_ACTIVATED);
        item.isActivating = (state == NM_ACTIVE_CONNECTION_STATE_ACTIVATING);
        item.iconName = iconForDevice(device);
        
        if (item.isActive)
            m_active.append(item);

        s_con = nm_connection_get_setting_connection(
            NM_CONNECTION(nm_active_connection_get_connection(ac)));
        ctype = s_con ? nm_setting_connection_get_connection_type(s_con) : 0;

        if (device && NM_IS_DEVICE_WIFI(device)) {
            if (item.isActive) {
                m_activeWifiUuid = item.uuid;
                m_hasActiveWifi = true;
                m_activeWifiItem = item;
                
                NMAccessPoint *ap = nm_device_wifi_get_active_access_point(NM_DEVICE_WIFI(device));
                if (ap) {
                    TQString ssid = ssidToString(nm_access_point_get_ssid(ap));
                    if (!ssid.isEmpty())
                        m_activeWifiItem.name = ssid;
                    m_activeWifiItem.iconName = NmIcons::signalStrengthIcon(
                        nm_access_point_get_strength(ap));
                    m_activeWifiItem.signalStrength =
                        nm_access_point_get_strength(ap);
                }
            } else if (item.isActivating) {
                m_hasActivatingWifi = true;
                m_activatingWifiItem = item;
                
                NMConnection *conn = NM_CONNECTION(nm_active_connection_get_connection(ac));
                if (conn) {
                    NMSettingWireless *s_wifi = nm_connection_get_setting_wireless(conn);
                    if (s_wifi) {
                        GBytes *ssid_bytes = nm_setting_wireless_get_ssid(s_wifi);
                        TQString ssid = ssidToString(ssid_bytes);
                        if (!ssid.isEmpty())
                            m_activatingWifiItem.name = ssid;
                    }
                }
            }
        } else if (ctype && strcmp(ctype, NM_SETTING_WIRED_SETTING_NAME) == 0) {
            m_hasActiveWired = true;
            m_activeWiredItem = item;
        }
    }
}

TQString connectionTypeLabel(const char *ctype)
{
    if (!ctype)
        return i18n("network");

    if (strcmp(ctype, NM_SETTING_WIRELESS_SETTING_NAME) == 0)
        return i18n("Wi-Fi");
    if (strcmp(ctype, NM_SETTING_WIRED_SETTING_NAME) == 0)
        return i18n("Wired");
    if (strcmp(ctype, NM_SETTING_VPN_SETTING_NAME) == 0)
        return i18n("VPN");

    return i18n("network");
}

void NmData::updatePrimaryConnection()
{
    NMClient *client = m_client ? m_client->nmClient() : 0;
    NMDevice *device = 0;
    NMActiveConnection *active = 0;
    NMSettingConnection *s_con;
    const char *ctype;

    m_hasActiveVpn = false;
    m_primaryConnectionName = TQString::null;
    m_primaryConnectionUuid = TQString::null;
    m_primaryConnectionTypeLabel = TQString::null;
    m_primaryConnectionCtype = TQString::null;

    if (!client)
        return;

    const GPtrArray *connections = nm_client_get_active_connections(client);
    for (guint i = 0; connections && i < connections->len; ++i) {
        NMActiveConnection *candidate = (NMActiveConnection *) g_ptr_array_index(connections, i);
        NMConnection *conn;

        if (nm_active_connection_get_state(candidate) != NM_ACTIVE_CONNECTION_STATE_ACTIVATED)
            continue;

        conn = NM_CONNECTION(nm_active_connection_get_connection(candidate));
        if (!conn)
            continue;

        s_con = nm_connection_get_setting_connection(conn);
        ctype = s_con ? nm_setting_connection_get_connection_type(s_con) : 0;
        if (ctype && strcmp(ctype, NM_SETTING_VPN_SETTING_NAME) == 0)
            m_hasActiveVpn = true;
    }

    active = resolvePrimaryActiveConnection(client, &device);
    if (!active
        || nm_active_connection_get_state(active) != NM_ACTIVE_CONNECTION_STATE_ACTIVATED)
        return;

    {
        const char *activeUuid = nm_active_connection_get_uuid(active);
        if (activeUuid)
            m_primaryConnectionUuid = TQString::fromUtf8(activeUuid);
    }

    s_con = nm_connection_get_setting_connection(
        NM_CONNECTION(nm_active_connection_get_connection(active)));
    ctype = s_con ? nm_setting_connection_get_connection_type(s_con) : 0;
    m_primaryConnectionTypeLabel = connectionTypeLabel(ctype);
    m_primaryConnectionCtype = ctype ? TQString::fromUtf8(ctype) : TQString::null;

    if (ctype && strcmp(ctype, NM_SETTING_WIRELESS_SETTING_NAME) == 0
        && m_hasActiveWifi && m_activeWifiItem.uuid == m_primaryConnectionUuid
        && !m_activeWifiItem.name.isEmpty()) {
        m_primaryConnectionName = m_activeWifiItem.name;
    } else if (ctype && strcmp(ctype, NM_SETTING_WIRED_SETTING_NAME) == 0
               && m_hasActiveWired && m_activeWiredItem.uuid == m_primaryConnectionUuid
               && !m_activeWiredItem.name.isEmpty()) {
        m_primaryConnectionName = m_activeWiredItem.name;
    } else {
        const char *id = nm_active_connection_get_id(active);
        if (id)
            m_primaryConnectionName = TQString::fromUtf8(id);
    }
}

TQString NmData::primaryConnectionIconName() const
{
    if (m_primaryConnectionName.isEmpty())
        return NmIcons::notConnectedIcon();

    if (m_primaryConnectionCtype == NM_SETTING_WIRELESS_SETTING_NAME) {
        if (m_hasActiveWifi && m_activeWifiItem.uuid == m_primaryConnectionUuid
            && !m_activeWifiItem.iconName.isEmpty())
            return m_activeWifiItem.iconName;
        return NmIcons::wirelessIcon();
    }
    if (m_primaryConnectionCtype == NM_SETTING_WIRED_SETTING_NAME)
        return NmIcons::wiredIcon();
    if (m_primaryConnectionCtype == NM_SETTING_VPN_SETTING_NAME)
        return NmIcons::vpnActiveIcon();

    return NmIcons::iconForConnectionType(m_primaryConnectionCtype.latin1());
}

static bool isSavedProfileActive(NMClient *client, const char *profilePath)
{
    const GPtrArray *connections;

    if (!client || !profilePath || !profilePath[0])
        return false;

    connections = nm_client_get_active_connections(client);
    for (guint i = 0; connections && i < connections->len; ++i) {
        NMActiveConnection *active = (NMActiveConnection *) g_ptr_array_index(connections, i);
        NMConnection *conn;

        if (nm_active_connection_get_state(active) != NM_ACTIVE_CONNECTION_STATE_ACTIVATED)
            continue;

        conn = NM_CONNECTION(nm_active_connection_get_connection(active));
        if (conn && strcmp(nm_connection_get_path(conn), profilePath) == 0)
            return true;
    }

    return false;
}

void NmData::rebuildSaved()
{
    m_wiredSaved.clear();
    m_vpnSaved.clear();
    m_primaryWiredIface = TQString::null;
    m_hasWiredDevice = false;
    m_isWiredCablePlugged = false;

    NMClient *client = m_client ? m_client->nmClient() : 0;
    if (!client)
        return;

    const GPtrArray *devices = nm_client_get_devices(client);
    for (guint d = 0; devices && d < devices->len; ++d) {
        NMDevice *dev = (NMDevice *) g_ptr_array_index(devices, d);
        if (NM_IS_DEVICE_ETHERNET(dev) && !nm_device_is_software(dev)) {
            m_hasWiredDevice = true;
            if (nm_device_ethernet_get_carrier(NM_DEVICE_ETHERNET(dev)))
                m_isWiredCablePlugged = true;
            if (m_primaryWiredIface.isEmpty() && isMenuVisibleDevice(dev))
                m_primaryWiredIface = TQString::fromUtf8(nm_device_get_iface(dev));
        }
    }

    const GPtrArray *connections = nm_client_get_connections(client);
    for (guint i = 0; connections && i < connections->len; ++i) {
        NMRemoteConnection *conn = (NMRemoteConnection *) g_ptr_array_index(connections, i);
        NMSettingConnection *s_con;
        const char *ctype;
        NmItem item;

        if (!isSavedConnectionVisible(conn))
            continue;

        s_con = nm_connection_get_setting_connection(NM_CONNECTION(conn));
        if (!s_con)
            continue;

        ctype = nm_setting_connection_get_connection_type(s_con);
        if (!ctype || strcmp(ctype, NM_SETTING_WIRELESS_SETTING_NAME) == 0)
            continue;

        if (strcmp(ctype, NM_SETTING_VPN_SETTING_NAME) == 0) {
            item.type = NmItem::Saved;
            item.name = TQString::fromUtf8(nm_setting_connection_get_id(s_con));
            item.uuid = TQString::fromUtf8(nm_setting_connection_get_uuid(s_con));
            item.path = TQString::fromUtf8(nm_connection_get_path(NM_CONNECTION(conn)));
            item.iconName = NmIcons::iconForConnectionType(ctype);
            item.isActive = isSavedProfileActive(
                client, nm_connection_get_path(NM_CONNECTION(conn)));
            m_vpnSaved.append(item);
            continue;
        }

        if (!isEthernetSavedConnection(conn))
            continue;

        item.type = NmItem::Saved;
        item.name = TQString::fromUtf8(nm_setting_connection_get_id(s_con));
        item.uuid = TQString::fromUtf8(nm_setting_connection_get_uuid(s_con));
        item.path = TQString::fromUtf8(nm_connection_get_path(NM_CONNECTION(conn)));
        item.iconName = NmIcons::iconForConnectionType(ctype);
        item.isActive = isSavedProfileActive(
            client, nm_connection_get_path(NM_CONNECTION(conn)));

        m_wiredSaved.append(item);
    }
}

void NmData::rebuildWifi()
{
    TQString activeApPath;
    TQString activeDisplayName;
    TQMap<TQString, NmItem> bestBySsid;
    TQMap<TQString, bool> ssidsInScan;
    TQValueList<SavedWifiProfile> savedProfiles;
    NMClient *client = m_client ? m_client->nmClient() : 0;
    NMDeviceWifi *wifiDev = 0;
    const GPtrArray *aps = 0;
    guint apCount = 0;
    NmItemList previousAvailable = m_wifiAvailable;

    m_wifiAvailable.clear();
    m_wifiSavedOutOfRange.clear();

    if (!client) {
        m_hasWifiDevice = false;
        m_primaryWifiDevice = TQString::null;
        m_primaryWifiIface = TQString::null;
        m_wifiNetworkCache.clear();
        m_wifiCacheMisses.clear();
        return;
    }

    wifiDev = pickPrimaryWifiDevice(client);
    if (wifiDev) {
        m_hasWifiDevice = true;
        m_primaryWifiDevice = TQString::fromUtf8(
            nm_object_get_path(NM_OBJECT(wifiDev)));
        m_primaryWifiIface = TQString::fromUtf8(nm_device_get_iface(NM_DEVICE(wifiDev)));
    } else {
        m_hasWifiDevice = false;
        m_primaryWifiDevice = TQString::null;
        m_primaryWifiIface = TQString::null;
        m_wifiNetworkCache.clear();
        m_wifiCacheMisses.clear();
        return;
    }

    if (!m_client->wirelessEnabled()) {
        m_wifiNetworkCache.clear();
        m_wifiCacheMisses.clear();
        return;
    }

    aps = nm_device_wifi_get_access_points(wifiDev);
    apCount = aps ? aps->len : 0;

    if (m_hasActiveWifi) {
        NMAccessPoint *activeAp = nm_device_wifi_get_active_access_point(wifiDev);
        if (activeAp) {
            TQString activeSsid = ssidToString(nm_access_point_get_ssid(activeAp));
            activeApPath = TQString::fromUtf8(nm_object_get_path(NM_OBJECT(activeAp)));
            activeDisplayName = activeSsid;
        } else if (!m_activeWifiItem.name.isEmpty()) {
            activeDisplayName = m_activeWifiItem.name;
        }
    }

    savedProfiles = loadSavedWifiProfiles(client);

    for (guint j = 0; aps && j < aps->len; ++j) {
        NMAccessPoint *ap = (NMAccessPoint *) g_ptr_array_index(aps, j);
        GBytes *ssidBytes = nm_access_point_get_ssid(ap);
        TQString apPath = TQString::fromUtf8(nm_object_get_path(NM_OBJECT(ap)));
        TQString ssid;
        TQString displayName;
        int strength = nm_access_point_get_strength(ap);
        guint32 flags = nm_access_point_get_flags(ap);
        guint32 wpa = nm_access_point_get_wpa_flags(ap);
        guint32 rsn = nm_access_point_get_rsn_flags(ap);
        bool secured = (wpa != NM_802_11_AP_SEC_NONE || rsn != NM_802_11_AP_SEC_NONE
                        || (flags & NM_802_11_AP_FLAGS_PRIVACY));
        NmItem item;

        if (isEmptySsidBytes(ssidBytes))
            continue;

        ssid = ssidToString(ssidBytes);
        displayName = ssid;
        ssidsInScan.insert(ssid, true);

        if (apPath == activeApPath)
            continue;
        if (m_hasActiveWifi && !activeDisplayName.isEmpty()
            && ssidsMatch(displayName, activeDisplayName))
            continue;

        {
            NMRemoteConnection *savedConn = savedConnectionForAccessPoint(
                client, NM_DEVICE(wifiDev), ap);

            if (savedConn) {
                NMSettingConnection *s_con = nm_connection_get_setting_connection(
                    NM_CONNECTION(savedConn));
                item.type = NmItem::Saved;
                item.name = displayName;
                item.path = TQString::fromUtf8(
                    nm_connection_get_path(NM_CONNECTION(savedConn)));
                item.uuid = s_con
                    ? TQString::fromUtf8(nm_setting_connection_get_uuid(s_con))
                    : TQString::null;
                item.hasSavedProfile = true;
            } else {
                item.type = NmItem::Wifi;
                item.name = displayName;
                item.path = apPath;
                item.uuid = TQString::null;
                item.hasSavedProfile = false;
            }
        }

        item.devicePath = m_primaryWifiDevice;
        item.specificObject = apPath;
        item.signalStrength = strength;
        item.secured = secured;
        item.iconName = NmIcons::signalStrengthIcon(strength);
        item.isActive = false;

        if (!bestBySsid.contains(displayName)
            || strength > bestBySsid[displayName].signalStrength)
            bestBySsid.insert(displayName, item);
    }

    /* Merge live APs into the persistent cache (nm-tray style). Never wipe on a
     * partial libnm snapshot that often contains only the connected BSS. */
    for (TQMap<TQString, NmItem>::ConstIterator it = bestBySsid.begin();
         it != bestBySsid.end(); ++it) {
        m_wifiNetworkCache.insert(it.key(), it.data());
        m_wifiCacheMisses.insert(it.key(), 0);
    }

    /* Prune only after a non-partial snapshot, and never while a scan session
     * is still filling the cache. */
    const bool allowPrune = !m_wifiScanActive && apCount > 1;
    if (allowPrune) {
        TQStringList stale;
        for (TQMap<TQString, NmItem>::ConstIterator it = m_wifiNetworkCache.begin();
             it != m_wifiNetworkCache.end(); ++it) {
            if (bestBySsid.contains(it.key()))
                continue;
            if (m_hasActiveWifi && !activeDisplayName.isEmpty()
                && ssidsMatch(it.key(), activeDisplayName)) {
                stale.append(it.key());
                continue;
            }
            int misses = m_wifiCacheMisses.contains(it.key())
                ? m_wifiCacheMisses[it.key()] + 1 : 1;
            if (misses >= 3)
                stale.append(it.key());
            else
                m_wifiCacheMisses.insert(it.key(), misses);
        }
        for (TQStringList::ConstIterator it = stale.begin(); it != stale.end(); ++it) {
            m_wifiNetworkCache.remove(*it);
            m_wifiCacheMisses.remove(*it);
        }
    }

    for (TQMap<TQString, NmItem>::ConstIterator it = m_wifiNetworkCache.begin();
         it != m_wifiNetworkCache.end(); ++it) {
        if (m_hasActiveWifi && !activeDisplayName.isEmpty()
            && ssidsMatch(it.key(), activeDisplayName))
            continue;
        
        NmItem availableItem = it.data();
        if (m_hasActivatingWifi && !m_activatingWifiItem.name.isEmpty()
            && ssidsMatch(availableItem.name, m_activatingWifiItem.name)) {
            availableItem.isActivating = true;
        }
        m_wifiAvailable.append(availableItem);
    }

    for (TQValueList<NmItem>::Iterator it1 = m_wifiAvailable.begin(); it1 != m_wifiAvailable.end(); ++it1) {
        for (TQValueList<NmItem>::Iterator it2 = it1; it2 != m_wifiAvailable.end(); ++it2) {
            bool it2Better = false;
            if ((*it2).isActivating && !(*it1).isActivating) {
                it2Better = true;
            } else if (!(*it2).isActivating && (*it1).isActivating) {
                it2Better = false;
            } else if ((*it2).signalStrength > (*it1).signalStrength) {
                it2Better = true;
            }
            
            if (it2Better) {
                NmItem tmp = *it1;
                *it1 = *it2;
                *it2 = tmp;
            }
        }
    }

    /* Cap the menu: 8 strongest APs total (active row + list), or 8 if none active.
     * Prefer previously shown SSIDs so the open menu does not reshuffle every refresh. */
    {
        const uint maxAvailable = m_hasActiveWifi ? 7u : 8u;
        if (m_wifiAvailable.size() > maxAvailable) {
            NmItemList capped;
            TQMap<TQString, bool> taken;

            for (uint i = 0; i < previousAvailable.size()
                 && capped.size() < maxAvailable; ++i) {
                const TQString &name = previousAvailable[i].name;
                for (uint j = 0; j < m_wifiAvailable.size(); ++j) {
                    if (m_wifiAvailable[j].name != name)
                        continue;
                    if (taken.contains(name))
                        break;
                    capped.append(m_wifiAvailable[j]);
                    taken.insert(name, true);
                    break;
                }
            }
            for (uint j = 0; j < m_wifiAvailable.size()
                 && capped.size() < maxAvailable; ++j) {
                if (taken.contains(m_wifiAvailable[j].name))
                    continue;
                capped.append(m_wifiAvailable[j]);
                taken.insert(m_wifiAvailable[j].name, true);
            }
            m_wifiAvailable = capped;
        }
    }

    {
        TQMap<TQString, bool> seenUuid;
        for (TQValueList<SavedWifiProfile>::ConstIterator it = savedProfiles.begin();
             it != savedProfiles.end(); ++it) {
            NmItem item;

            if ((*it).uuid == m_activeWifiUuid)
                continue;
            if (seenUuid.contains((*it).uuid))
                continue;
            if (m_wifiNetworkCache.contains((*it).ssid))
                continue;
            if (ssidsInScan.contains((*it).ssid))
                continue;

            seenUuid.insert((*it).uuid, true);
            item.type = NmItem::Saved;
            item.name = (*it).name;
            item.uuid = (*it).uuid;
            item.path = (*it).path;
            item.devicePath = m_primaryWifiDevice;
            item.iconName = NmIcons::wirelessOffIcon();
            item.isActive = false;
            item.hasSavedProfile = true;
            m_wifiSavedOutOfRange.append(item);
        }
    }
}

TQString NmData::wifiMenuFingerprint() const
{
    TQStringList parts;

    /* SSIDs / uuids only — signal strength churn must not rebuild the menu. */
    for (uint i = 0; i < m_wifiAvailable.size(); ++i) {
        parts.append(TQString("A:%1:%2")
            .arg(m_wifiAvailable[i].name)
            .arg(m_wifiAvailable[i].uuid));
    }
    for (uint i = 0; i < m_wifiSavedOutOfRange.size(); ++i) {
        parts.append(TQString("S:%1").arg(m_wifiSavedOutOfRange[i].uuid));
    }

    return parts.join("|");
}

void NmData::refresh()
{
    rebuildActive();
    updatePrimaryConnection();
    rebuildSaved();
    rebuildWifi();
    emit refreshed();
}

bool NmData::isTrayConnecting() const
{
    if (!m_client || !m_client->isNmRunning())
        return false;

    NMClient *client = m_client->nmClient();
    if (!client)
        return false;

    if (m_client->clientState() == NM_STATE_CONNECTING)
        return true;
    if (anyActivatingConnection(client))
        return true;
    return anyDeviceConnecting(client);
}

bool NmData::shouldNotifyConnectionLost() const
{
    if (isTrayConnecting())
        return false;

    if (!m_client || !m_client->isNmRunning())
        return true;

    NMClient *client = m_client->nmClient();
    if (!client)
        return true;

    const GPtrArray *devices = nm_client_get_devices(client);
    for (guint i = 0; devices && i < devices->len; ++i) {
        NMDevice *device = (NMDevice *) g_ptr_array_index(devices, i);

        if (!isRelevantDevice(device))
            continue;

        if (nm_device_get_state(device) == NM_DEVICE_STATE_DEACTIVATING)
            return false;
    }

    switch (m_client->clientState()) {
    case NM_STATE_CONNECTING:
        return false;
    case NM_STATE_DISCONNECTED:
    case NM_STATE_ASLEEP:
        return true;
    default:
        /* CONNECTED_* but no primary profile yet: Wi-Fi handoff, not a real drop. */
        return false;
    }
}

TQString NmData::trayIconName() const
{
    if (!m_client || !m_client->isNmRunning())
        return NmIcons::notConnectedIcon();

    switch (m_client->clientState()) {
    case NM_STATE_UNKNOWN:
    case NM_STATE_ASLEEP:
        return NmIcons::notConnectedIcon();
    case NM_STATE_DISCONNECTED:
        return NmIcons::notConnectedIcon();
    default:
        break;
    }

    NMClient *client = m_client ? m_client->nmClient() : 0;
    NMDevice *device = 0;
    NMActiveConnection *active = resolvePrimaryActiveConnection(client, &device);

    TQString icon;
    if (m_hasActiveVpn && active
        && nm_active_connection_get_state(active) == NM_ACTIVE_CONNECTION_STATE_ACTIVATED)
        icon = NmIcons::vpnActiveIcon();
    else
        icon = iconForDevice(device);

    // We don't override the icon with disconnected just because of NM connectivity check,
    // as it confuses users who are locally connected but NM's check fails or is disabled.

    return icon;
}

TQString NmData::trayTooltip() const
{
    if (!m_client || !m_client->isNmRunning())
        return i18n("NetworkManager is not running");

    switch (m_client->clientState()) {
    case NM_STATE_ASLEEP:
        return i18n("Networking disabled");
    case NM_STATE_DISCONNECTED:
        return i18n("No network connection");
    default:
        break;
    }

    NMClient *client = m_client ? m_client->nmClient() : 0;
    NMDevice *device = 0;
    NMActiveConnection *active = resolvePrimaryActiveConnection(client, &device);

    return tooltipForDevice(device, active);
}

bool NmData::connectWifiAvailable(int index, TQString *errorOut)
{
    if (index < 0 || index >= (int) m_wifiAvailable.size()) {
        if (errorOut)
            *errorOut = i18n("Network list is out of date. Please reopen the menu.");
        return false;
    }

    if (m_wifiScanActive)
        finishWifiScanSession();

    return activateWifiMenuItem(m_client, m_wifiAvailable[index], errorOut);
}

bool NmData::connectWifiSavedOutOfRange(int index, TQString *errorOut)
{
    if (index < 0 || index >= (int) m_wifiSavedOutOfRange.size()) {
        if (errorOut)
            *errorOut = i18n("Network list is out of date. Please reopen the menu.");
        return false;
    }

    if (m_wifiScanActive)
        finishWifiScanSession();

    NmItem item = m_wifiSavedOutOfRange[index];
    NmEventPump::pump();
    return activateWifiMenuItem(m_client, item, errorOut);
}

void NmData::activateItem(const NmItem &item)
{
    if (!m_client)
        return;

    if (item.type == NmItem::Active)
        return;

    if (item.type == NmItem::Wifi) {
        if (m_hasActivatingWifi && !m_activatingWifiItem.name.isEmpty() && ssidsMatch(item.name, m_activatingWifiItem.name))
            return;
        m_client->abortActivatingConnections(true);
        if (m_wifiScanActive)
            finishWifiScanSession();
        activateWifiMenuItem(m_client, item);
        return;
    }

    if (item.type == NmItem::Saved) {
        NMClient *client = m_client->nmClient();
        NMRemoteConnection *conn = 0;
        const char *ctype = 0;

        if (client && !item.path.isEmpty()) {
            conn = nm_client_get_connection_by_path(
                client, item.path.utf8().data());
            if (conn) {
                NMSettingConnection *s_con = nm_connection_get_setting_connection(
                    NM_CONNECTION(conn));
                if (s_con)
                    ctype = nm_setting_connection_get_connection_type(s_con);
            }
        }

        if (ctype && strcmp(ctype, NM_SETTING_WIRELESS_SETTING_NAME) == 0) {
            if (m_hasActivatingWifi && !m_activatingWifiItem.path.isEmpty() && m_activatingWifiItem.path == item.path)
                return;
            m_client->abortActivatingConnections(true);
            if (m_wifiScanActive)
                finishWifiScanSession();
            activateWifiMenuItem(m_client, item);
            return;
        }

        if (ctype && (strcmp(ctype, NM_SETTING_VPN_SETTING_NAME) == 0 ||
                      strcmp(ctype, NM_SETTING_WIREGUARD_SETTING_NAME) == 0)) {
            m_client->activateConnection(item.path, TQString::fromLatin1("/"));
            return;
        }

        NMDevice *device = 0;
        if (!client)
            return;

        if (ctype && strcmp(ctype, NM_SETTING_WIRED_SETTING_NAME) == 0) {
            // No granular tracking for which wired connection is activating, so we abort blindly before launching a new one
            m_client->abortActivatingConnections(false);

            const GPtrArray *devices = nm_client_get_devices(client);
            for (guint i = 0; devices && i < devices->len; ++i) {
                NMDevice *d = NM_DEVICE(g_ptr_array_index(devices, i));
                if (NM_IS_DEVICE_ETHERNET(d) && !nm_device_is_software(d)) {
                    device = d;
                    break;
                }
            }
        } else {
            device = bestTrayDevice(client);
        }

        if (!device)
            return;

        m_client->activateConnection(item.path,
                                     TQString::fromUtf8(
                                         nm_object_get_path(NM_OBJECT(device))));
    }
}

void NmData::deactivateItem(const NmItem &item)
{
    if (!m_client)
        return;

    if (!item.uuid.isEmpty() && m_client->deactivateActiveConnection(item.uuid))
        return;

    if (!item.path.isEmpty() && m_client->deactivateActiveConnection(item.path))
        return;

    TQString devicePath = item.devicePath;
    if (devicePath.isEmpty())
        devicePath = m_primaryWifiDevice;
    if (devicePath.isEmpty())
        return;

    m_client->deactivateDevice(devicePath);
}

void NmData::deactivateWifi()
{
    if (!m_client)
        return;

    if (m_client->deactivateWifiConnections())
        return;

    if (m_hasActiveWifi)
        deactivateItem(m_activeWifiItem);
    else if (!m_primaryWifiDevice.isEmpty())
        m_client->deactivateDevice(m_primaryWifiDevice);
}

void NmData::requestWifiScan()
{
    NMClient *client = m_client ? m_client->nmClient() : 0;
    const GPtrArray *devices;

    if (!client || !m_client->wirelessEnabled())
        return;

    /* nm-applet: request a scan on every Wi-Fi device, not only the primary. */
    devices = nm_client_get_devices(client);
    for (guint i = 0; devices && i < devices->len; ++i) {
        NMDevice *device = (NMDevice *) g_ptr_array_index(devices, i);
        const char *iface;

        if (!NM_IS_DEVICE_WIFI(device))
            continue;
        if (!isMenuVisibleDevice(device))
            continue;

        iface = nm_device_get_iface(device);
        if (iface && strncmp(iface, "p2p-dev-", 8) == 0)
            continue;

        /* Must use D-Bus object path, not nm_device_get_path() (udev). */
        m_client->requestWifiScan(TQString::fromUtf8(
            nm_object_get_path(NM_OBJECT(device))));
    }
}

static const gint64 kWifiScanMaxWaitMs = 15000;

void NmData::startWifiScanSession(bool userRequested)
{
    if (userRequested)
        m_wifiScanUserRequested = true;
    else if (!m_wifiScanActive)
        m_wifiScanUserRequested = false;

    m_wifiScanActive = true;
    /* Always stamp the request time so we wait for a NEW last-scan, like nmcli. */
    m_wifiScanStartedMs = (long long) nm_utils_get_timestamp_msec();
}

void NmData::startWifiScanOnMenuOpen()
{
    NMClient *client = m_client ? m_client->nmClient() : 0;

    if (!client || !m_client->wirelessEnabled())
        return;

    /* Only auto-scan when we have virtually nothing to show. Otherwise the
     * user triggers a refresh via "Wifi - request scan". */
    if (m_wifiNetworkCache.size() > 1 || !primaryWifiListSparse(client))
        return;

    startWifiScanSession(false);
    requestWifiScan();
}

void NmData::finishWifiScanSession()
{
    m_wifiScanActive = false;
    m_wifiScanUserRequested = false;
    m_wifiScanStartedMs = 0;
}

bool NmData::wifiScanAwaitingLastScan() const
{
    NMClient *client = m_client ? m_client->nmClient() : 0;
    NMDeviceWifi *wifi;

    if (!m_wifiScanActive || !client)
        return false;

    wifi = pickPrimaryWifiDevice(client);
    if (!wifi)
        return false;

    return wifiDeviceLastScanMs(wifi) < (gint64) m_wifiScanStartedMs;
}

bool NmData::wifiScanSessionComplete() const
{
    NMClient *client = m_client ? m_client->nmClient() : 0;
    NMDeviceWifi *wifi;
    gint64 lastScan;
    gint64 now;

    if (!m_wifiScanActive)
        return true;

    if (!client || !m_client->wirelessEnabled())
        return true;

    wifi = pickPrimaryWifiDevice(client);
    if (!wifi)
        return true;

    now = nm_utils_get_timestamp_msec();
    lastScan = wifiDeviceLastScanMs(wifi);

    /* Done once LastScan advanced past the request (APs arrive via signals). */
    if (lastScan >= (gint64) m_wifiScanStartedMs)
        return true;

    if (now >= (gint64) m_wifiScanStartedMs + kWifiScanMaxWaitMs)
        return true;

    return false;
}

bool NmData::maybeStartBackgroundWifiScan()
{
    NMClient *client = m_client ? m_client->nmClient() : 0;

    if (!client || !m_client->wirelessEnabled())
        return false;

    if (m_wifiScanActive)
        return true;

    if (m_wifiNetworkCache.size() > 1 || !primaryWifiListSparse(client))
        return false;

    startWifiScanSession(false);
    requestWifiScan();
    return true;
}

bool NmData::isHiddenWifiSecurityAvailable(WifiSecurityType type) const
{
    NMClient *client = m_client ? m_client->nmClient() : 0;
    NMDeviceWifi *wifi = 0;

    if (!client)
        return wifiSecurityAvailable(type, 0);

    wifi = pickPrimaryWifiDevice(client);
    return wifiSecurityAvailable(type, wifi);
}

static bool wifiSharePermissionAllowed(NMClient *client, NMClientPermission permission)
{
    NMClientPermissionResult result;

    if (!client)
        return false;

    result = nm_client_get_permission_result(client, permission);
    return result == NM_CLIENT_PERMISSION_RESULT_YES
        || result == NM_CLIENT_PERMISSION_RESULT_AUTH;
}

bool NmData::canCreateWifiHotspot() const
{
    NMClient *client = m_client ? m_client->nmClient() : 0;

    if (!client || !m_client->isNmRunning())
        return false;
    if (!m_client->networkingEnabled())
        return false;
    if (!m_client->wirelessHardwareEnabled() || !m_client->wirelessEnabled())
        return false;
    if (!pickApCapableWifiDevice(client))
        return false;

    return wifiSharePermissionAllowed(client, NM_CLIENT_PERMISSION_WIFI_SHARE_OPEN)
        || wifiSharePermissionAllowed(client, NM_CLIENT_PERMISSION_WIFI_SHARE_PROTECTED);
}

bool NmData::isCreateWifiSecurityAvailable(WifiSecurityType type) const
{
    NMClient *client = m_client ? m_client->nmClient() : 0;
    NMDeviceWifi *wifi;

    if (!client)
        return false;

    wifi = pickApCapableWifiDevice(client);
    if (!wifi)
        return false;

    switch (type) {
    case WifiSecNone:
        return wifiSharePermissionAllowed(client, NM_CLIENT_PERMISSION_WIFI_SHARE_OPEN);
    case WifiSecWpaPsk:
    case WifiSecSae:
        if (!wifiSharePermissionAllowed(client, NM_CLIENT_PERMISSION_WIFI_SHARE_PROTECTED))
            return false;
        return wifiSecurityAvailable(type, wifi);
    default:
        return false;
    }
}

bool NmData::connectHiddenWifi(const TQString &ssid, WifiSecurityType security,
                               const TQString &identity, const TQString &secret,
                               TQString *errorOut)
{
    NMClient *client = m_client ? m_client->nmClient() : 0;
    NMDevice *device = 0;
    NMDeviceWifi *wifi = 0;
    NMConnection *newConn = 0;
    NMSettingConnection *s_con = 0;
    NMSettingWireless *s_wifi = 0;
    GBytes *ssidBytes = 0;
    TQCString ssidUtf8;

    if (!client) {
        if (errorOut)
            *errorOut = i18n("NetworkManager is not available.");
        return false;
    }

    if (!m_client->wirelessEnabled()) {
        if (errorOut)
            *errorOut = i18n("Wi-Fi is disabled.");
        return false;
    }

    NmEventPump::pump();

    wifi = pickPrimaryWifiDevice(client);
    if (!wifi) {
        if (errorOut)
            *errorOut = i18n("No Wi-Fi device available.");
        return false;
    }
    device = NM_DEVICE(wifi);

    if (m_wifiScanActive)
        finishWifiScanSession();
    m_client->deactivateWifiOnDevice(device);
    NmEventPump::pumpRepeated(5);

    if (!wifiSecurityAvailable(security, wifi)) {
        if (errorOut)
            *errorOut = i18n("This security type is not supported by the Wi-Fi device.");
        return false;
    }

    ssidUtf8 = ssid.utf8();
    newConn = nm_simple_connection_new();
    s_con = NM_SETTING_CONNECTION(nm_setting_connection_new());
    s_wifi = NM_SETTING_WIRELESS(nm_setting_wireless_new());

    g_object_set(G_OBJECT(s_con),
                 NM_SETTING_CONNECTION_ID, ssidUtf8.data(),
                 NM_SETTING_CONNECTION_TYPE, NM_SETTING_WIRELESS_SETTING_NAME,
                 NM_SETTING_CONNECTION_AUTOCONNECT, FALSE,
                 NULL);

    ssidBytes = g_bytes_new(ssidUtf8.data(), ssidUtf8.length());
    g_object_set(G_OBJECT(s_wifi),
                 NM_SETTING_WIRELESS_SSID, ssidBytes,
                 NM_SETTING_WIRELESS_MODE, NM_SETTING_WIRELESS_MODE_INFRA,
                 NM_SETTING_WIRELESS_HIDDEN, TRUE,
                 NULL);
    g_bytes_unref(ssidBytes);

    nm_connection_add_setting(newConn, NM_SETTING(s_con));
    nm_connection_add_setting(newConn, NM_SETTING(s_wifi));

    if (!wifiSecurityApply(newConn, security, identity, secret, errorOut)) {
        g_object_unref(newConn);
        return false;
    }

    nm_client_add_and_activate_connection_async(
        client, newConn, device, "/", NULL, NULL, NULL);
    g_object_unref(newConn);
    return true;
}

bool NmData::createWifiHotspot(const TQString &ssid, WifiSecurityType security,
                               const TQString &password, TQString *errorOut)
{
    NMClient *client = m_client ? m_client->nmClient() : 0;
    NMDevice *device = 0;
    NMDeviceWifi *wifi = 0;
    NMConnection *newConn = 0;
    NMSettingConnection *s_con = 0;
    NMSettingWireless *s_wifi = 0;
    NMSettingIP4Config *s_ip4 = 0;
    NMSettingIP6Config *s_ip6 = 0;
    GBytes *ssidBytes = 0;
    TQCString ssidUtf8;
    char *uuid = 0;

    if (!client) {
        if (errorOut)
            *errorOut = i18n("NetworkManager is not available.");
        return false;
    }

    if (!m_client->networkingEnabled()) {
        if (errorOut)
            *errorOut = i18n("Networking is disabled.");
        return false;
    }

    if (!m_client->wirelessEnabled()) {
        if (errorOut)
            *errorOut = i18n("Wi-Fi is disabled.");
        return false;
    }

    if (ssid.stripWhiteSpace().isEmpty()) {
        if (errorOut)
            *errorOut = i18n("Please enter the network name (SSID).");
        return false;
    }

    if (!isCreateWifiSecurityAvailable(security)) {
        if (errorOut)
            *errorOut = i18n("This security type is not available for Wi-Fi hotspot creation.");
        return false;
    }

    NmEventPump::pump();

    wifi = pickApCapableWifiDevice(client);
    if (!wifi) {
        if (errorOut)
            *errorOut = i18n("No Wi-Fi device supporting Access Point mode was found.");
        return false;
    }
    device = NM_DEVICE(wifi);

    if (m_wifiScanActive)
        finishWifiScanSession();
    m_client->deactivateWifiOnDevice(device);
    NmEventPump::pumpRepeated(5);

    ssidUtf8 = ssid.utf8();
    newConn = nm_simple_connection_new();
    s_con = NM_SETTING_CONNECTION(nm_setting_connection_new());
    s_wifi = NM_SETTING_WIRELESS(nm_setting_wireless_new());
    s_ip4 = NM_SETTING_IP4_CONFIG(nm_setting_ip4_config_new());
    s_ip6 = NM_SETTING_IP6_CONFIG(nm_setting_ip6_config_new());

    uuid = nm_utils_uuid_generate();
    g_object_set(G_OBJECT(s_con),
                 NM_SETTING_CONNECTION_ID, ssidUtf8.data(),
                 NM_SETTING_CONNECTION_UUID, uuid,
                 NM_SETTING_CONNECTION_TYPE, NM_SETTING_WIRELESS_SETTING_NAME,
                 NM_SETTING_CONNECTION_AUTOCONNECT, FALSE,
                 NULL);
    g_free(uuid);

    ssidBytes = g_bytes_new(ssidUtf8.data(), ssidUtf8.length());
    g_object_set(G_OBJECT(s_wifi),
                 NM_SETTING_WIRELESS_SSID, ssidBytes,
                 NM_SETTING_WIRELESS_MODE, NM_SETTING_WIRELESS_MODE_AP,
                 NULL);
    g_bytes_unref(ssidBytes);

    g_object_set(G_OBJECT(s_ip4),
                 NM_SETTING_IP_CONFIG_METHOD, NM_SETTING_IP4_CONFIG_METHOD_SHARED,
                 NULL);
    g_object_set(G_OBJECT(s_ip6),
                 NM_SETTING_IP_CONFIG_METHOD, NM_SETTING_IP6_CONFIG_METHOD_IGNORE,
                 NULL);

    nm_connection_add_setting(newConn, NM_SETTING(s_con));
    nm_connection_add_setting(newConn, NM_SETTING(s_wifi));
    nm_connection_add_setting(newConn, NM_SETTING(s_ip4));
    nm_connection_add_setting(newConn, NM_SETTING(s_ip6));

    if (!wifiSecurityApply(newConn, security, TQString(), password, errorOut)) {
        g_object_unref(newConn);
        return false;
    }

    nm_client_add_and_activate_connection_async(
        client, newConn, device, NULL, NULL, NULL, NULL);
    g_object_unref(newConn);
    NmEventPump::pumpAfterAsync();
    return true;
}

bool NmData::wiredEnabled() const
{
    return m_client ? m_client->wiredEnabled() : false;
}

void NmData::setWiredEnabled(bool enabled)
{
    if (m_client) {
        m_client->setWiredEnabled(enabled);
        NmEventPump::pumpAfterAsync();
    }
}

void NmData::deactivateWired()
{
    if (!m_client)
        return;

    NMClient *client = m_client->nmClient();
    if (!client)
        return;

    const GPtrArray *active = nm_client_get_active_connections(client);
    for (guint i = 0; active && i < active->len; ++i) {
        NMActiveConnection *ac = NM_ACTIVE_CONNECTION(g_ptr_array_index(active, i));
        const GPtrArray *devices = nm_active_connection_get_devices(ac);
        if (devices && devices->len > 0) {
            NMDevice *d = NM_DEVICE(g_ptr_array_index(devices, 0));
            if (NM_IS_DEVICE_ETHERNET(d) && !nm_device_is_software(d)) {
                nm_device_disconnect(d, NULL, NULL);
            }
        }
    }
    NmEventPump::pumpAfterAsync();
}

#include "nmdata.moc"
