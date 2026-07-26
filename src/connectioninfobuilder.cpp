#include "connectioninfobuilder.h"
#include "icons.h"
#include "wifiutil.h"
#include "nm/glib_compat.h"

#include <tdelocale.h>
#include <tqstringlist.h>

#include <string.h>
#include <stdio.h>

namespace {

void appendSection(TQString &text, const TQString &title)
{
    if (!text.isEmpty())
        text += "\n";
    text += title;
    text += "\n";
    text += "========================================\n";
}

void appendRow(TQString &text, const TQString &label, const TQString &value)
{
    text += label;
    text += ": ";
    text += value.isEmpty() ? i18n("unknown") : value;
    text += "\n";
}

TQString deviceTypeLabel(NMDeviceType type)
{
    switch (type) {
    case NM_DEVICE_TYPE_ETHERNET:
        return i18n("Ethernet");
    case NM_DEVICE_TYPE_WIFI:
        return i18n("Wi-Fi");
    case NM_DEVICE_TYPE_BT:
        return i18n("Bluetooth");
    case NM_DEVICE_TYPE_OLPC_MESH:
        return i18n("OLPC Mesh");
    case NM_DEVICE_TYPE_WIMAX:
        return i18n("WiMAX");
    case NM_DEVICE_TYPE_MODEM:
        return i18n("Modem");
    case NM_DEVICE_TYPE_INFINIBAND:
        return i18n("InfiniBand");
    case NM_DEVICE_TYPE_BOND:
        return i18n("Bond");
    case NM_DEVICE_TYPE_VLAN:
        return i18n("VLAN");
    case NM_DEVICE_TYPE_ADSL:
        return i18n("ADSL");
    case NM_DEVICE_TYPE_BRIDGE:
        return i18n("Bridge");
    case NM_DEVICE_TYPE_GENERIC:
        return i18n("Generic");
    case NM_DEVICE_TYPE_TEAM:
        return i18n("Team");
    case NM_DEVICE_TYPE_TUN:
        return i18n("TUN");
    case NM_DEVICE_TYPE_IP_TUNNEL:
        return i18n("IP Tunnel");
    case NM_DEVICE_TYPE_MACVLAN:
        return i18n("MACVLAN");
    case NM_DEVICE_TYPE_VXLAN:
        return i18n("VXLAN");
    case NM_DEVICE_TYPE_VETH:
        return i18n("VETH");
    case NM_DEVICE_TYPE_MACSEC:
        return i18n("MACsec");
    case NM_DEVICE_TYPE_DUMMY:
        return i18n("Dummy");
    case NM_DEVICE_TYPE_PPP:
        return i18n("PPP");
    case NM_DEVICE_TYPE_OVS_INTERFACE:
        return i18n("Open vSwitch");
    case NM_DEVICE_TYPE_OVS_PORT:
        return i18n("Open vSwitch Port");
    case NM_DEVICE_TYPE_OVS_BRIDGE:
        return i18n("Open vSwitch Bridge");
    case NM_DEVICE_TYPE_WPAN:
        return i18n("WPAN");
    case NM_DEVICE_TYPE_6LOWPAN:
        return i18n("6LoWPAN");
    case NM_DEVICE_TYPE_WIREGUARD:
        return i18n("WireGuard");
    case NM_DEVICE_TYPE_WIFI_P2P:
        return i18n("Wi-Fi P2P");
    default:
        return i18n("Unknown");
    }
}

TQString formatBitrate(guint32 bitRate)
{
    if (bitRate == 0)
        return i18n("unknown");

    if (bitRate <= 1000)
        return i18n("%1 Kb/s").arg(bitRate);

    if (bitRate <= 1000000) {
        double mb = (double) bitRate / 1000.0;
        return i18n("%1 Mb/s").arg(mb, 0, 'g', 5);
    }

    if (bitRate <= 1000000000) {
        double gb = (double) bitRate / 1000000.0;
        return i18n("%1 Gb/s").arg(gb, 0, 'g', 5);
    }

    double tb = (double) bitRate / 1000000000.0;
    return i18n("%1 Tb/s").arg(tb, 0, 'g', 5);
}

TQString formatHwAddress(const char *addr)
{
    if (!addr || !addr[0])
        return TQString::null;
    return TQString::fromUtf8(addr);
}

TQString prefixToNetmask(guint prefix)
{
    if (prefix == 0 || prefix > 32)
        return TQString::null;

    guint32 mask = 0xFFFFFFFFu << (32 - prefix);
    return TQString("%1.%2.%3.%4")
        .arg((mask >> 24) & 0xFF)
        .arg((mask >> 16) & 0xFF)
        .arg((mask >> 8) & 0xFF)
        .arg(mask & 0xFF);
}

TQString formatBssid(const char *bssid)
{
    if (!bssid || !bssid[0])
        return TQString::null;
    return TQString::fromUtf8(bssid);
}

bool isVirtualIfaceName(const char *iface)
{
    if (!iface)
        return false;

    if (strcmp(iface, "lo") == 0)
        return true;
    return isSystemVirtualIfaceName(iface);
}

bool isRelevantActiveConnection(NMActiveConnection *ac)
{
    NMConnection *conn;
    NMSettingConnection *s_con;
    const char *ctype;
    const GPtrArray *devices;
    NMDevice *device;

    if (!ac)
        return false;

    if (nm_active_connection_get_state(ac) != NM_ACTIVE_CONNECTION_STATE_ACTIVATED)
        return false;

    conn = NM_CONNECTION(nm_active_connection_get_connection(ac));
    s_con = conn ? nm_connection_get_setting_connection(conn) : 0;
    ctype = s_con ? nm_setting_connection_get_connection_type(s_con) : 0;
    if (!ctype)
        return false;

    if (strcmp(ctype, NM_SETTING_WIRELESS_SETTING_NAME) == 0)
        return true;
    if (strcmp(ctype, NM_SETTING_WIRED_SETTING_NAME) == 0)
        return true;
    if (strcmp(ctype, NM_SETTING_VPN_SETTING_NAME) == 0)
        return true;
    if (strcmp(ctype, NM_SETTING_WIREGUARD_SETTING_NAME) == 0)
        return true;
    if (strcmp(ctype, NM_SETTING_GSM_SETTING_NAME) == 0)
        return true;
    if (strcmp(ctype, NM_SETTING_CDMA_SETTING_NAME) == 0)
        return true;

    devices = nm_active_connection_get_devices(ac);
    if (!devices || !devices->len)
        return false;

    device = (NMDevice *) g_ptr_array_index(devices, 0);
    if (!device)
        return false;

    switch (nm_device_get_device_type(device)) {
    case NM_DEVICE_TYPE_ETHERNET:
    case NM_DEVICE_TYPE_WIFI:
    case NM_DEVICE_TYPE_MODEM:
    case NM_DEVICE_TYPE_BT:
    case NM_DEVICE_TYPE_WIREGUARD:
        break;
    default:
        return false;
    }

    return !isVirtualIfaceName(nm_device_get_iface(device));
}

NMActiveConnection *primaryRelevantActiveConnection(NMClient *client)
{
    NMActiveConnection *fallback = 0;
    const GPtrArray *connections;
    guint i;

    if (!client)
        return 0;

    connections = nm_client_get_active_connections(client);
    for (i = 0; connections && i < connections->len; ++i) {
        NMActiveConnection *candidate =
            (NMActiveConnection *) g_ptr_array_index(connections, i);

        if (!isRelevantActiveConnection(candidate))
            continue;

        if (nm_active_connection_get_default(candidate))
            return candidate;

        if (!fallback)
            fallback = candidate;
    }

    return fallback;
}

TQString iconNameForActiveConnection(NMActiveConnection *ac)
{
    NMConnection *conn;
    NMSettingConnection *s_con;
    const char *ctype;

    if (!ac)
        return NmIcons::notConnectedIcon();

    conn = NM_CONNECTION(nm_active_connection_get_connection(ac));
    s_con = conn ? nm_connection_get_setting_connection(conn) : 0;
    ctype = s_con ? nm_setting_connection_get_connection_type(s_con) : 0;
    if (!ctype)
        return NmIcons::notConnectedIcon();

    if (strcmp(ctype, NM_SETTING_WIRELESS_SETTING_NAME) == 0)
        return NmIcons::wirelessIcon();
    if (strcmp(ctype, NM_SETTING_WIRED_SETTING_NAME) == 0)
        return NmIcons::wiredIcon();
    if (strcmp(ctype, NM_SETTING_VPN_SETTING_NAME) == 0)
        return NmIcons::vpnActiveIcon();
    if (strcmp(ctype, NM_SETTING_WIREGUARD_SETTING_NAME) == 0)
        return NmIcons::vpnActiveIcon();

    return NmIcons::notConnectedIcon();
}

void appendIpConfigSection(TQString &text, const TQString &title, NMIPConfig *config)
{
    const GPtrArray *addresses;
    const char *const *nameservers;
    const char *const *domains;
    const char *gateway;
    guint i;

    if (!config || !nm_ip_config_get_family(config))
        return;

    appendSection(text, title);

    addresses = nm_ip_config_get_addresses(config);
    for (i = 0; addresses && i < addresses->len; ++i) {
        NMIPAddress *addr = (NMIPAddress *) g_ptr_array_index(addresses, i);
        TQString suffix = (i > 0)
            ? TQString(" (%1)").arg(i + 1) : TQString::null;
        TQString ip = TQString::fromUtf8(nm_ip_address_get_address(addr));
        guint prefix = nm_ip_address_get_prefix(addr);
        int family = nm_ip_address_get_family(addr);

        appendRow(text, i18n("IP Address") + suffix, ip);
        if (family == AF_INET)
            appendRow(text, i18n("Subnet Mask") + suffix, prefixToNetmask(prefix));
        else
            appendRow(text, i18n("Prefix") + suffix, TQString::number(prefix));
    }

    gateway = nm_ip_config_get_gateway(config);
    if (gateway && gateway[0])
        appendRow(text, i18n("Default route"), TQString::fromUtf8(gateway));

    nameservers = nm_ip_config_get_nameservers(config);
    for (i = 0; nameservers && nameservers[i]; ++i) {
        appendRow(text, i18n("DNS(%1)").arg(i + 1),
                  TQString::fromUtf8(nameservers[i]));
    }

    domains = nm_ip_config_get_domains(config);
    if (domains && domains[0]) {
        TQStringList parts;
        for (i = 0; domains[i]; ++i)
            parts.append(TQString::fromUtf8(domains[i]));
        appendRow(text, i18n("DNS search"), parts.join(", "));
    }
}

TQString buildInfoForActiveConnection(NMActiveConnection *ac, NMClient *client)
{
    TQString text;
    NMConnection *conn;
    NMDevice *device = 0;
    const GPtrArray *devices;
    const char *id;
    const char *ctype;
    TQString hwAddress;
    guint32 bitRate = 0;
    TQString security;

    if (!ac)
        return text;

    conn = NM_CONNECTION(nm_active_connection_get_connection(ac));
    NMSettingConnection *s_con = conn
        ? nm_connection_get_setting_connection(conn) : 0;
    id = nm_active_connection_get_id(ac);

    devices = nm_active_connection_get_devices(ac);
    if (devices && devices->len)
        device = (NMDevice *) g_ptr_array_index(devices, 0);

    appendSection(text, i18n("General"));
    if (id && id[0])
        appendRow(text, i18n("Connection"), TQString::fromUtf8(id));

    ctype = s_con ? nm_setting_connection_get_connection_type(s_con) : 0;
    if (ctype && ctype[0])
        appendRow(text, i18n("Type"), TQString::fromUtf8(ctype));

    if (!device) {
        appendRow(text, i18n("State"), i18n("Activated"));
        return text;
    }

    appendRow(text, i18n("Interface"),
              deviceTypeLabel(nm_device_get_device_type(device))
              + TQString(" (") + TQString::fromUtf8(nm_device_get_iface(device))
              + TQString(")"));

    if (NM_IS_DEVICE_ETHERNET(device)) {
        hwAddress = formatHwAddress(nm_device_get_hw_address(device));
        bitRate = nm_device_ethernet_get_speed(NM_DEVICE_ETHERNET(device));
    } else if (NM_IS_DEVICE_WIFI(device)) {
        NMDeviceWifi *wifi = NM_DEVICE_WIFI(device);
        hwAddress = formatHwAddress(nm_device_get_hw_address(device));
        bitRate = nm_device_wifi_get_bitrate(wifi);

        if (conn) {
            NMSettingWirelessSecurity *s_wsec =
                nm_connection_get_setting_wireless_security(conn);
            const char *keyMgmt = s_wsec
                ? nm_setting_wireless_security_get_key_mgmt(s_wsec) : 0;
            if (keyMgmt && keyMgmt[0])
                security = TQString::fromUtf8(keyMgmt);
        }

        NMAccessPoint *ap = nm_device_wifi_get_active_access_point(wifi);
        if (ap) {
            TQString ssid = ssidToString(nm_access_point_get_ssid(ap));
            if (!ssid.isEmpty())
                appendRow(text, i18n("Network"), ssid);
            appendRow(text, i18n("Signal strength"),
                      i18n("%1%").arg(nm_access_point_get_strength(ap)));
            appendRow(text, i18n("Frequency"),
                      i18n("%1 MHz").arg(nm_access_point_get_frequency(ap)));
            appendRow(text, i18n("BSSID"),
                      formatBssid(nm_access_point_get_bssid(ap)));
        }
    } else {
        const char *hw = nm_device_get_hw_address(device);
        hwAddress = formatHwAddress(hw);
    }

    appendRow(text, i18n("Hardware Address"), hwAddress);
    appendRow(text, i18n("Driver"),
              TQString::fromUtf8(nm_device_get_driver(device)));
    appendRow(text, i18n("Speed"), formatBitrate(bitRate));

    if (!security.isEmpty())
        appendRow(text, i18n("Security"), security);

    appendIpConfigSection(text, i18n("IPv4"),
                          nm_device_get_ip4_config(device));
    appendIpConfigSection(text, i18n("IPv6"),
                          nm_device_get_ip6_config(device));

    (void) client;
    return text;
}

} // namespace

TQValueList<ConnectionInfoEntry> buildActiveConnectionInfoEntries(NMClient *client)
{
    TQValueList<ConnectionInfoEntry> entries;
    NMActiveConnection *ac;
    ConnectionInfoEntry entry;
    const char *id;

    if (!client)
        return entries;

    ac = primaryRelevantActiveConnection(client);
    if (!ac)
        return entries;

    id = nm_active_connection_get_id(ac);
    entry.tabTitle = (id && id[0])
        ? TQString::fromUtf8(id) : i18n("Connection");
    entry.iconName = iconNameForActiveConnection(ac);
    entry.body = buildInfoForActiveConnection(ac, client);
    if (!entry.body.isEmpty())
        entries.append(entry);

    return entries;
}

bool hasActiveConnectionInfo(NMClient *client)
{
    return primaryRelevantActiveConnection(client) != 0;
}
