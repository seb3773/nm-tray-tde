#include "connectioneditorutil.h"
#include "wifiutil.h"
#include "icons.h"
#include "nm/nmeventpump.h"
#include "nm/glib_compat.h"

#include <tdelocale.h>
#include <tqdialog.h>
#include <tqapplication.h>
#include <tqdesktopwidget.h>
#include <tqevent.h>
#include <tqlayout.h>

#include <string.h>

typedef struct {
    NMRemoteConnection *remote;
    GError *error;
    gboolean done;
} AddConnectionData;

static void onAddConnectionDone(GObject *source, GAsyncResult *result, gpointer user_data)
{
    AddConnectionData *data = (AddConnectionData *) user_data;

    data->remote = nm_client_add_connection_finish(NM_CLIENT(source), result, &data->error);
    data->done = TRUE;
}

void centerEditorDialog(TQDialog *dialog)
{
    if (!dialog)
        return;

    TQWidget *ref = dialog->parentWidget() ? dialog->parentWidget() : dialog;
    TQDesktopWidget *desktop = TQApplication::desktop();
    int screen = desktop->screenNumber(ref);
    TQRect area = desktop->availableGeometry(screen);
    int x = area.x() + (area.width() - dialog->width()) / 2;
    int y = area.y() + (area.height() - dialog->height()) / 2;

    if (dialog->parentWidget() && !dialog->isTopLevel())
        dialog->move(dialog->parentWidget()->mapFromGlobal(TQPoint(x, y)));
    else
        dialog->move(x, y);
}

bool commitConnectionProfile(NMRemoteConnection *remote, TQString *errorOut)
{
    GError *error = 0;

    if (!remote) {
        if (errorOut)
            *errorOut = i18n("Connection not found.");
        return false;
    }

    if (!nm_remote_connection_commit_changes(remote, TRUE, NULL, &error)) {
        if (errorOut) {
            *errorOut = error && error->message
                ? TQString::fromUtf8(error->message)
                : i18n("Failed to save connection.");
        }
        if (error)
            g_error_free(error);
        return false;
    }

    NmEventPump::pump();
    return true;
}

bool addConnectionProfile(NMClient *client, NMConnection *connection,
                          TQString *pathOut, TQString *errorOut)
{
    AddConnectionData data;

    if (!client || !connection) {
        if (errorOut)
            *errorOut = i18n("Invalid connection profile.");
        return false;
    }

    data.remote = 0;
    data.error = 0;
    data.done = FALSE;

    nm_client_add_connection_async(client, connection, TRUE, NULL,
                                   onAddConnectionDone, &data);
    while (!data.done) {
        g_main_context_iteration(NULL, FALSE);
        tqApp->processEvents();
        g_usleep(1000);
    }

    if (!data.remote) {
        if (errorOut) {
            *errorOut = data.error && data.error->message
                ? TQString::fromUtf8(data.error->message)
                : i18n("Failed to add connection.");
        }
        if (data.error)
            g_error_free(data.error);
        return false;
    }

    if (pathOut) {
        *pathOut = TQString::fromUtf8(
            nm_connection_get_path(NM_CONNECTION(data.remote)));
    }

    g_object_unref(data.remote);
    NmEventPump::pump();
    return true;
}

NMConnection *newWifiConnectionTemplate()
{
    NMConnection *conn = nm_simple_connection_new();
    NMSettingConnection *s_con = NM_SETTING_CONNECTION(nm_setting_connection_new());
    NMSettingWireless *s_wifi = NM_SETTING_WIRELESS(nm_setting_wireless_new());

    g_object_set(G_OBJECT(s_con),
                 NM_SETTING_CONNECTION_ID, "New Wi-Fi",
                 NM_SETTING_CONNECTION_TYPE, NM_SETTING_WIRELESS_SETTING_NAME,
                 NM_SETTING_CONNECTION_AUTOCONNECT, FALSE,
                 NULL);
    g_object_set(G_OBJECT(s_wifi),
                 NM_SETTING_WIRELESS_MODE, NM_SETTING_WIRELESS_MODE_INFRA,
                 NULL);

    nm_connection_add_setting(conn, NM_SETTING(s_con));
    nm_connection_add_setting(conn, NM_SETTING(s_wifi));
    return conn;
}

NMConnection *newWiredConnectionTemplate()
{
    NMConnection *conn = nm_simple_connection_new();
    NMSettingConnection *s_con = NM_SETTING_CONNECTION(nm_setting_connection_new());
    NMSettingWired *s_wired = NM_SETTING_WIRED(nm_setting_wired_new());

    g_object_set(G_OBJECT(s_con),
                 NM_SETTING_CONNECTION_ID, "New Wired",
                 NM_SETTING_CONNECTION_TYPE, NM_SETTING_WIRED_SETTING_NAME,
                 NM_SETTING_CONNECTION_AUTOCONNECT, TRUE,
                 NULL);

    nm_connection_add_setting(conn, NM_SETTING(s_con));
    nm_connection_add_setting(conn, NM_SETTING(s_wired));
    return conn;
}

static const char *defaultVpnServerKey(const char *service)
{
    if (service && strstr(service, "l2tp"))
        return "gateway";
    if (service && strstr(service, "pptp"))
        return "gateway";
    return "remote";
}

NMConnection *newVpnConnectionTemplate(const char *serviceType)
{
    NMConnection *conn = nm_simple_connection_new();
    NMSettingConnection *s_con = NM_SETTING_CONNECTION(nm_setting_connection_new());
    const char *service = serviceType ? serviceType
        : "org.freedesktop.NetworkManager.openvpn";

    if (service && strstr(service, "wireguard")) {
        NMSettingWireGuard *s_wg = NM_SETTING_WIREGUARD(nm_setting_wireguard_new());
        NMSettingIP4Config *s_ip4 = NM_SETTING_IP4_CONFIG(nm_setting_ip4_config_new());

        g_object_set(G_OBJECT(s_con),
                     NM_SETTING_CONNECTION_ID, "New VPN",
                     NM_SETTING_CONNECTION_TYPE, NM_SETTING_WIREGUARD_SETTING_NAME,
                     NM_SETTING_CONNECTION_AUTOCONNECT, FALSE,
                     NULL);
        g_object_set(G_OBJECT(s_ip4),
                     NM_SETTING_IP_CONFIG_METHOD, NM_SETTING_IP4_CONFIG_METHOD_DISABLED,
                     NULL);

        nm_connection_add_setting(conn, NM_SETTING(s_con));
        nm_connection_add_setting(conn, NM_SETTING(s_wg));
        nm_connection_add_setting(conn, NM_SETTING(s_ip4));
        return conn;
    }

    NMSettingVpn *s_vpn = NM_SETTING_VPN(nm_setting_vpn_new());

    g_object_set(G_OBJECT(s_con),
                 NM_SETTING_CONNECTION_ID, "New VPN",
                 NM_SETTING_CONNECTION_TYPE, NM_SETTING_VPN_SETTING_NAME,
                 NM_SETTING_CONNECTION_AUTOCONNECT, FALSE,
                 NULL);
    g_object_set(G_OBJECT(s_vpn),
                 NM_SETTING_VPN_SERVICE_TYPE, service,
                 NULL);
    nm_setting_vpn_add_data_item(s_vpn, defaultVpnServerKey(service), "");

    nm_connection_add_setting(conn, NM_SETTING(s_con));
    nm_connection_add_setting(conn, NM_SETTING(s_vpn));
    return conn;
}

bool isWirelessConnection(NMConnection *conn)
{
    NMSettingConnection *s_con;

    if (!conn)
        return false;

    s_con = nm_connection_get_setting_connection(conn);
    if (s_con) {
        const char *ctype = nm_setting_connection_get_connection_type(s_con);
        if (ctype && strcmp(ctype, NM_SETTING_WIRELESS_SETTING_NAME) == 0)
            return true;
    }

    return nm_connection_get_setting_wireless(conn) != 0;
}

bool isWiredConnection(NMConnection *conn)
{
    NMSettingConnection *s_con;

    if (!conn)
        return false;

    s_con = nm_connection_get_setting_connection(conn);
    if (s_con) {
        const char *ctype = nm_setting_connection_get_connection_type(s_con);
        if (ctype && strcmp(ctype, NM_SETTING_WIRED_SETTING_NAME) == 0)
            return true;
    }

    return nm_connection_get_setting_wired(conn) != 0;
}

bool isConnectionEditorVisible(NMConnection *conn)
{
    NMSettingConnection *s_con;
    const char *ctype;
    const char *iface;
    const char *id;

    if (!conn)
        return false;

    s_con = nm_connection_get_setting_connection(conn);
    if (!s_con)
        return false;

    if (nm_setting_connection_get_master(s_con))
        return false;

    ctype = nm_setting_connection_get_connection_type(s_con);
    if (!ctype)
        return false;

    /* Loopback stays visible by design. */
    if (strcmp(ctype, NM_SETTING_LOOPBACK_SETTING_NAME) == 0)
        return true;

    if (strcmp(ctype, NM_SETTING_VLAN_SETTING_NAME) == 0)
        return false;
    if (strcmp(ctype, NM_SETTING_BRIDGE_PORT_SETTING_NAME) == 0)
        return false;
    if (strcmp(ctype, NM_SETTING_BOND_PORT_SETTING_NAME) == 0)
        return false;
    if (strcmp(ctype, NM_SETTING_TEAM_PORT_SETTING_NAME) == 0)
        return false;
    if (strcmp(ctype, NM_SETTING_BLUETOOTH_SETTING_NAME) == 0)
        return false;

    iface = nm_setting_connection_get_interface_name(s_con);
    id = nm_setting_connection_get_id(s_con);
    if (isSystemVirtualIfaceName(iface) || isSystemVirtualIfaceName(id))
        return false;

    return true;
}

ConnectionProfileUi connectionProfileUi(NMConnection *conn)
{
    ConnectionProfileUi ui;
    NMSettingConnection *s_con;
    const char *ctype;

    ui.typeLabel = i18n("Unknown");
    ui.iconName = NmIcons::notConnectedIcon();

    if (!conn)
        return ui;

    s_con = nm_connection_get_setting_connection(conn);
    ctype = s_con ? nm_setting_connection_get_connection_type(s_con) : 0;

    if (isWirelessConnection(conn)) {
        ui.typeLabel = i18n("Wi-Fi");
        ui.iconName = NmIcons::wirelessIcon();
    } else if (isWiredConnection(conn)) {
        ui.typeLabel = i18n("Wired");
        ui.iconName = NmIcons::wiredIcon();
    } else if (isVpnConnection(conn)) {
        ui.typeLabel = i18n("VPN");
        ui.iconName = NmIcons::vpnActiveIcon();
    } else if (ctype && strcmp(ctype, NM_SETTING_BRIDGE_SETTING_NAME) == 0) {
        ui.typeLabel = i18n("Wired");
        ui.iconName = NmIcons::wiredIcon();
    } else if (ctype && strcmp(ctype, NM_SETTING_LOOPBACK_SETTING_NAME) == 0) {
        ui.typeLabel = i18n("Loopback");
        ui.iconName = NmIcons::loopbackIcon();
    } else if (ctype) {
        ui.typeLabel = TQString::fromUtf8(ctype);
        ui.iconName = NmIcons::iconForConnectionType(ctype);
    }

    return ui;
}

bool isVpnConnection(NMConnection *conn)
{
    NMSettingConnection *s_con;

    if (!conn)
        return false;

    s_con = nm_connection_get_setting_connection(conn);
    if (s_con) {
        const char *ctype = nm_setting_connection_get_connection_type(s_con);
        if (ctype) {
            if (strcmp(ctype, NM_SETTING_VPN_SETTING_NAME) == 0)
                return true;
            if (strcmp(ctype, NM_SETTING_WIREGUARD_SETTING_NAME) == 0)
                return true;
        }
    }

    return nm_connection_get_setting_vpn(conn) != 0
        || connectionWireguardSetting(conn) != 0;
}

NMSettingWireGuard *connectionWireguardSetting(NMConnection *conn)
{
    NMSetting *setting;

    if (!conn)
        return 0;

    setting = nm_connection_get_setting_by_name(conn, NM_SETTING_WIREGUARD_SETTING_NAME);
    return setting ? NM_SETTING_WIREGUARD(setting) : 0;
}
