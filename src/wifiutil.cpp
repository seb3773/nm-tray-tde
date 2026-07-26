#include "wifiutil.h"
#include "nm/glib_compat.h"

#include <tdelocale.h>

#include <string.h>

namespace {

} // namespace

bool isRelevantDevice(NMDevice *device)
{
    if (!device)
        return false;
        
    if (!nm_device_get_managed(device))
        return false;
        
    if (nm_device_is_software(device))
        return false;

    switch (nm_device_get_device_type(device)) {
    case NM_DEVICE_TYPE_ETHERNET:
    case NM_DEVICE_TYPE_WIFI:
    case NM_DEVICE_TYPE_MODEM:
    case NM_DEVICE_TYPE_BT:
        return true;
    default:
        return false;
    }
}

TQString ssidToString(GBytes *ssid)
{
    if (!ssid)
        return TQString();

    gsize len = 0;
    const guint8 *data = (const guint8 *) g_bytes_get_data(ssid, &len);
    if (!data || !len)
        return TQString();

    return TQString::fromUtf8((const char *) data, len);
}

bool ssidsMatch(const TQString &ssid, const TQString &profileSsid)
{
    if (ssid == i18n("(hidden)"))
        return profileSsid.isEmpty();
    return ssid == profileSsid;
}

bool isEmptySsidBytes(GBytes *ssid)
{
    gsize len = 0;
    const guint8 *data;

    if (!ssid)
        return true;

    data = (const guint8 *) g_bytes_get_data(ssid, &len);
    if (!data || !len)
        return true;

    for (gsize i = 0; i < len; ++i) {
        if (data[i] != 0)
            return false;
    }
    return true;
}

bool isSystemVirtualIfaceName(const char *iface)
{
    if (!iface || !*iface)
        return false;

    /* System / container / hypervisor clutter — never show in tray or editor.
     * Loopback (lo) is intentionally NOT included: keep it visible in Edit Connections. */
    if (strncmp(iface, "docker", 6) == 0)
        return true;
    if (strncmp(iface, "virbr", 5) == 0)
        return true;
    if (strncmp(iface, "veth", 4) == 0)
        return true;
    if (strncmp(iface, "br-", 3) == 0)
        return true;

    return false;
}

bool isMenuVisibleDevice(NMDevice *device)
{
    const char *iface;

    if (!isRelevantDevice(device))
        return false;

    switch (nm_device_get_device_type(device)) {
    case NM_DEVICE_TYPE_LOOPBACK:
    case NM_DEVICE_TYPE_BRIDGE:
        return false;
    default:
        break;
    }

    iface = nm_device_get_iface(device);
    if (!iface)
        return true;

    if (strcmp(iface, "lo") == 0)
        return false;
    if (isSystemVirtualIfaceName(iface))
        return false;

    return true;
}

NMDeviceWifi *pickPrimaryWifiDevice(NMClient *client)
{
    NMDeviceWifi *best = 0;
    guint bestApCount = 0;
    bool bestActivated = false;
    const GPtrArray *devices = nm_client_get_devices(client);

    for (guint d = 0; devices && d < devices->len; ++d) {
        NMDevice *device = (NMDevice *) g_ptr_array_index(devices, d);
        NMDeviceWifi *wifi;
        const GPtrArray *aps;
        guint apCount;
        bool activated;
        const char *iface;

        if (!NM_IS_DEVICE_WIFI(device))
            continue;

        if (!isMenuVisibleDevice(device))
            continue;

        iface = nm_device_get_iface(device);
        if (iface && strncmp(iface, "p2p-dev-", 8) == 0)
            continue;

        wifi = NM_DEVICE_WIFI(device);
        aps = nm_device_wifi_get_access_points(wifi);
        apCount = aps ? aps->len : 0;
        activated = (nm_device_get_state(device) == NM_DEVICE_STATE_ACTIVATED);

        if (!best) {
            best = wifi;
            bestApCount = apCount;
            bestActivated = activated;
            continue;
        }

        if (activated && !bestActivated) {
            best = wifi;
            bestApCount = apCount;
            bestActivated = true;
        } else if (activated == bestActivated && apCount > bestApCount) {
            best = wifi;
            bestApCount = apCount;
        }
    }

    return best;
}

NMDeviceWifi *pickApCapableWifiDevice(NMClient *client)
{
    NMDeviceWifi *primary;
    const GPtrArray *devices;

    if (!client)
        return 0;

    primary = pickPrimaryWifiDevice(client);
    if (primary) {
        guint32 caps = nm_device_wifi_get_capabilities(primary);
        if (caps & NM_WIFI_DEVICE_CAP_AP)
            return primary;
    }

    devices = nm_client_get_devices(client);
    for (guint d = 0; devices && d < devices->len; ++d) {
        NMDevice *device = (NMDevice *) g_ptr_array_index(devices, d);
        NMDeviceWifi *wifi;
        const char *iface;
        guint32 caps;

        if (!NM_IS_DEVICE_WIFI(device))
            continue;
        if (!isMenuVisibleDevice(device))
            continue;

        iface = nm_device_get_iface(device);
        if (iface && strncmp(iface, "p2p-dev-", 8) == 0)
            continue;

        wifi = NM_DEVICE_WIFI(device);
        caps = nm_device_wifi_get_capabilities(wifi);
        if (caps & NM_WIFI_DEVICE_CAP_AP)
            return wifi;
    }

    return 0;
}

NMAccessPoint *accessPointForSsid(NMDeviceWifi *wifi, const TQString &ssid)
{
    const GPtrArray *aps;

    if (!wifi)
        return 0;

    aps = nm_device_wifi_get_access_points(wifi);
    for (guint i = 0; aps && i < aps->len; ++i) {
        NMAccessPoint *ap = (NMAccessPoint *) g_ptr_array_index(aps, i);
        GBytes *ssidBytes = nm_access_point_get_ssid(ap);
        gsize len = 0;
        const guint8 *data;
        TQString apSsid;

        if (isEmptySsidBytes(ssidBytes))
            continue;

        data = (const guint8 *) g_bytes_get_data(ssidBytes, &len);
        if (!data || !len)
            continue;

        apSsid = TQString::fromUtf8((const char *) data, len);
        if (ssidsMatch(ssid, apSsid))
            return ap;
    }

    return 0;
}

NMRemoteConnection *savedConnectionForAccessPoint(NMClient *client,
                                                  NMDevice *device,
                                                  NMAccessPoint *ap)
{
    const GPtrArray *all;
    GPtrArray *deviceConnections = 0;
    GPtrArray *apConnections = 0;
    NMRemoteConnection *result = 0;

    if (!client || !device || !ap)
        return 0;

    all = nm_client_get_connections(client);
    deviceConnections = nm_device_filter_connections(device, all);
    if (!deviceConnections)
        return 0;

    apConnections = nm_access_point_filter_connections(ap, deviceConnections);
    g_ptr_array_unref(deviceConnections);

    if (apConnections && apConnections->len)
        result = NM_REMOTE_CONNECTION(g_ptr_array_index(apConnections, 0));

    if (apConnections)
        g_ptr_array_unref(apConnections);

    return result;
}

NMRemoteConnection *savedConnectionForSsidOnDevice(NMDevice *device,
                                                   const TQString &ssid)
{
    const GPtrArray *available;

    if (!device)
        return 0;

    available = nm_device_get_available_connections(device);
    for (guint i = 0; available && i < available->len; ++i) {
        NMRemoteConnection *conn = NM_REMOTE_CONNECTION(g_ptr_array_index(available, i));
        NMSettingConnection *s_con = nm_connection_get_setting_connection(NM_CONNECTION(conn));
        NMSettingWireless *s_wifi = nm_connection_get_setting_wireless(NM_CONNECTION(conn));
        GBytes *ssidBytes;
        gsize len = 0;
        const guint8 *data;
        TQString profileSsid;

        if (!s_con || !s_wifi)
            continue;

        ssidBytes = nm_setting_wireless_get_ssid(s_wifi);
        data = ssidBytes ? (const guint8 *) g_bytes_get_data(ssidBytes, &len) : 0;
        if (data && len)
            profileSsid = TQString::fromUtf8((const char *) data, len);

        TQString profileId = TQString::fromUtf8(nm_setting_connection_get_id(s_con));
        if (ssidsMatch(ssid, profileSsid) || ssid == profileId)
            return conn;
    }

    return 0;
}
