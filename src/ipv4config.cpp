#include "ipv4config.h"

#include "nm/glib_compat.h"

#include <tdelocale.h>
#include <tqhostaddress.h>
#include <tqstringlist.h>

#include <string.h>

static TQString prefixToNetmask(guint prefix)
{
    guint32 mask = nm_utils_ip4_prefix_to_netmask(prefix);

    return TQString("%1.%2.%3.%4")
        .arg((mask >> 24) & 0xff)
        .arg((mask >> 16) & 0xff)
        .arg((mask >> 8) & 0xff)
        .arg(mask & 0xff);
}

static bool netmaskToPrefix(const TQString &netmask, guint *prefixOut)
{
    TQHostAddress addr(netmask);
    guint32 mask;

    if (netmask.isEmpty() || addr.isNull())
        return false;

    mask = addr.toIPv4Address();
    if (!mask)
        return false;

    *prefixOut = nm_utils_ip4_netmask_to_prefix(mask);
    return true;
}

static TQString joinDnsList(NMSettingIPConfig *s_ip)
{
    TQStringList parts;
    guint count = nm_setting_ip_config_get_num_dns(s_ip);

    for (guint i = 0; i < count; ++i) {
        const char *dns = nm_setting_ip_config_get_dns(s_ip, (int) i);
        if (dns && dns[0])
            parts.append(TQString::fromUtf8(dns));
    }

    return parts.join(" ");
}

static TQString joinDnsSearchList(NMSettingIPConfig *s_ip)
{
    TQStringList parts;
    guint count = nm_setting_ip_config_get_num_dns_searches(s_ip);

    for (guint i = 0; i < count; ++i) {
        const char *domain = nm_setting_ip_config_get_dns_search(s_ip, (int) i);
        if (domain && domain[0])
            parts.append(TQString::fromUtf8(domain));
    }

    return parts.join(" ");
}

static TQStringList splitTokens(const TQString &text)
{
    TQStringList parts;
    TQString item;
    int pos = 0;

    while (pos < (int) text.length()) {
        while (pos < (int) text.length() && text[pos].isSpace())
            ++pos;
        if (pos >= (int) text.length())
            break;
        item = TQString::null;
        while (pos < (int) text.length() && !text[pos].isSpace()) {
            item += text[pos];
            ++pos;
        }
        if (!item.isEmpty())
            parts.append(item);
    }

    return parts;
}

void ipv4EditorLoad(NMConnection *conn, Ipv4EditorState *state)
{
    NMSettingIPConfig *s_ip4;
    const char *method;

    if (!conn || !state)
        return;

    state->manual = false;
    state->address = TQString::null;
    state->netmask = TQString::null;
    state->gateway = TQString::null;
    state->dns = TQString::null;
    state->dnsSearch = TQString::null;

    s_ip4 = nm_connection_get_setting_ip4_config(conn);
    if (!s_ip4)
        return;

    method = nm_setting_ip_config_get_method(NM_SETTING_IP_CONFIG(s_ip4));
    state->manual = (method && strcmp(method, NM_SETTING_IP4_CONFIG_METHOD_MANUAL) == 0);

    if (state->manual && nm_setting_ip_config_get_num_addresses(s_ip4) > 0) {
        NMIPAddress *addr = nm_setting_ip_config_get_address(s_ip4, 0);
        if (addr) {
            const char *ip = nm_ip_address_get_address(addr);
            if (ip)
                state->address = TQString::fromUtf8(ip);
            state->netmask = prefixToNetmask(nm_ip_address_get_prefix(addr));
        }
    }

    if (nm_setting_ip_config_get_gateway(s_ip4)) {
        state->gateway = TQString::fromUtf8(nm_setting_ip_config_get_gateway(s_ip4));
    }

    state->dns = joinDnsList(s_ip4);
    state->dnsSearch = joinDnsSearchList(s_ip4);
}

bool ipv4EditorApply(NMConnection *conn, const Ipv4EditorState &state, TQString *errorOut)
{
    NMSettingIPConfig *s_ip4;
    GError *error = 0;

    if (!conn)
        return false;

    s_ip4 = nm_connection_get_setting_ip4_config(conn);
    if (!s_ip4) {
        s_ip4 = NM_SETTING_IP_CONFIG(nm_setting_ip4_config_new());
        nm_connection_add_setting(conn, NM_SETTING(s_ip4));
    }

    if (!state.manual) {
        g_object_set(G_OBJECT(s_ip4),
                     NM_SETTING_IP_CONFIG_METHOD, NM_SETTING_IP4_CONFIG_METHOD_AUTO,
                     NULL);
        nm_setting_ip_config_clear_addresses(s_ip4);
        g_object_set(G_OBJECT(s_ip4), NM_SETTING_IP_CONFIG_GATEWAY, NULL, NULL);
        nm_setting_ip_config_clear_dns(s_ip4);
        nm_setting_ip_config_clear_dns_searches(s_ip4);
        return true;
    }

    TQString ip = state.address.stripWhiteSpace();
    TQString netmask = state.netmask.stripWhiteSpace();
    TQString gateway = state.gateway.stripWhiteSpace();
    guint prefix = 24;
    NMIPAddress *addr = 0;

    if (ip.isEmpty()) {
        if (errorOut)
            *errorOut = i18n("Please enter an IP address.");
        return false;
    }

    if (!netmaskToPrefix(netmask, &prefix)) {
        TQHostAddress ipAddr(ip);
        if (!ipAddr.isNull())
            prefix = nm_utils_ip4_get_default_prefix(ipAddr.toIPv4Address());
        else {
            if (errorOut)
                *errorOut = i18n("Please enter a valid netmask.");
            return false;
        }
    }

    g_object_set(G_OBJECT(s_ip4),
                 NM_SETTING_IP_CONFIG_METHOD, NM_SETTING_IP4_CONFIG_METHOD_MANUAL,
                 NULL);

    nm_setting_ip_config_clear_addresses(s_ip4);
    addr = nm_ip_address_new(AF_INET, ip.utf8().data(), prefix, &error);
    if (!addr) {
        if (errorOut) {
            *errorOut = error && error->message
                ? TQString::fromUtf8(error->message)
                : i18n("Invalid IP address.");
        }
        if (error)
            g_error_free(error);
        return false;
    }
    nm_setting_ip_config_add_address(s_ip4, addr);
    nm_ip_address_unref(addr);

    if (gateway.isEmpty())
        g_object_set(G_OBJECT(s_ip4), NM_SETTING_IP_CONFIG_GATEWAY, NULL, NULL);
    else
        g_object_set(G_OBJECT(s_ip4), NM_SETTING_IP_CONFIG_GATEWAY, gateway.utf8().data(), NULL);

    nm_setting_ip_config_clear_dns(s_ip4);
    TQStringList dnsList = splitTokens(state.dns);
    for (uint i = 0; i < dnsList.size(); ++i) {
        TQHostAddress dnsAddr(dnsList[i]);
        if (dnsAddr.isNull())
            continue;
        nm_setting_ip_config_add_dns(s_ip4, dnsList[i].utf8().data());
    }

    nm_setting_ip_config_clear_dns_searches(s_ip4);
    TQStringList searchList = splitTokens(state.dnsSearch);
    for (uint i = 0; i < searchList.size(); ++i) {
        if (searchList[i].isEmpty())
            continue;
        nm_setting_ip_config_add_dns_search(s_ip4, searchList[i].utf8().data());
    }

    return true;
}
