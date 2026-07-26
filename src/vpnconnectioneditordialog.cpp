#include "vpnconnectioneditordialog.h"
#include "connectioneditorutil.h"
#include "icons.h"
#include "nm/nmclient.h"
#include "nm/glib_compat.h"

#include <tdelocale.h>
#include <tqlabel.h>
#include <tqlineedit.h>
#include <tqpushbutton.h>
#include <tqcheckbox.h>
#include <tqgroupbox.h>
#include <tqlayout.h>
#include <tdemessagebox.h>
#include <tqevent.h>

#include <string.h>

namespace {

static TQString vpnServiceLabel(const char *service)
{
    if (!service || !service[0])
        return i18n("VPN");

    if (strstr(service, "openvpn"))
        return i18n("OpenVPN");
    if (strstr(service, "vpnc"))
        return i18n("Cisco VPN");
    if (strstr(service, "l2tp"))
        return i18n("L2TP");
    if (strstr(service, "pptp"))
        return i18n("PPTP");
    if (strstr(service, "openconnect"))
        return i18n("OpenConnect");
    if (strstr(service, "wireguard"))
        return i18n("WireGuard");
    if (strstr(service, "fortisslvpn"))
        return i18n("Fortinet SSL VPN");
    if (strstr(service, "iodine"))
        return i18n("iodine");

    return TQString::fromUtf8(service);
}

static const char *defaultServerKey(const char *service)
{
    if (service && strstr(service, "l2tp"))
        return "gateway";
    if (service && strstr(service, "pptp"))
        return "gateway";
    if (service && strstr(service, "openconnect"))
        return "gateway";
    if (service && strstr(service, "vpnc"))
        return "gateway";
    return "remote";
}

static TQString vpnDataValue(NMSettingVpn *s_vpn, const char *key)
{
    const char *value;

    if (!s_vpn || !key)
        return TQString();

    value = nm_setting_vpn_get_data_item(s_vpn, key);
    if (value && value[0])
        return TQString::fromUtf8(value);
    return TQString();
}

static TQString findVpnDataValue(NMSettingVpn *s_vpn, const char *const *keys)
{
    for (int i = 0; keys[i]; ++i) {
        TQString value = vpnDataValue(s_vpn, keys[i]);
        if (!value.isEmpty())
            return value;
    }
    return TQString();
}

static TQString findVpnDataKey(NMSettingVpn *s_vpn, const char *const *keys)
{
    for (int i = 0; keys[i]; ++i) {
        const char *value = nm_setting_vpn_get_data_item(s_vpn, keys[i]);
        if (value && value[0])
            return TQString::fromUtf8(keys[i]);
    }
    return TQString();
}

static TQString findVpnSecretKey(NMSettingVpn *s_vpn, const char *const *preferred)
{
    guint len = 0;
    const char **existing = nm_setting_vpn_get_secret_keys(s_vpn, &len);

    for (guint i = 0; existing && i < len; ++i) {
        for (int j = 0; preferred[j]; ++j) {
            if (strcmp(existing[i], preferred[j]) == 0)
                return TQString::fromUtf8(existing[i]);
        }
    }

    if (preferred[0])
        return TQString::fromUtf8(preferred[0]);
    return TQString::fromUtf8("password");
}

static void setVpnDataItem(NMSettingVpn *s_vpn, const char *key, const TQString &value)
{
    if (!s_vpn || !key)
        return;

    nm_setting_vpn_remove_data_item(s_vpn, key);
    if (!value.isEmpty())
        nm_setting_vpn_add_data_item(s_vpn, key, value.utf8().data());
}

static void setVpnSecret(NMSettingVpn *s_vpn, const char *key, const TQString &value)
{
    if (!s_vpn || !key || value.isEmpty())
        return;

    nm_setting_vpn_remove_secret(s_vpn, key);
    nm_setting_vpn_add_secret(s_vpn, key, value.utf8().data());
}

static TQString allowedIpsToEdit(const NMWireGuardPeer *peer)
{
    TQStringList parts;
    guint count;

    if (!peer)
        return TQString();

    count = nm_wireguard_peer_get_allowed_ips_len(peer);
    for (guint i = 0; i < count; ++i) {
        gboolean valid = FALSE;
        const char *ip = nm_wireguard_peer_get_allowed_ip(peer, i, &valid);
        if (ip && ip[0])
            parts.append(TQString::fromUtf8(ip));
    }

    return parts.join("; ");
}

static bool applyAllowedIps(NMWireGuardPeer *peer, const TQString &text)
{
    TQStringList parts = TQStringList::split(";", text, true);
    guint i;

    if (!peer)
        return false;

    nm_wireguard_peer_clear_allowed_ips(peer);
    for (i = 0; i < parts.size(); ++i) {
        TQString part = parts[i].stripWhiteSpace();
        if (part.isEmpty())
            continue;
        if (!nm_wireguard_peer_append_allowed_ip(peer, part.utf8().data(), TRUE))
            return false;
    }
    return true;
}

} // namespace

void VpnConnectionEditorDialog::addEditorRow(TQWidget *parent, TQVBoxLayout *root,
                                             const TQString &label, EditorRow *row,
                                             bool password)
{
    TQHBoxLayout *layout = new TQHBoxLayout();
    layout->setSpacing(6);
    row->label = new TQLabel(label, parent);
    layout->addWidget(row->label);
    row->edit = new TQLineEdit(parent);
    if (password)
        row->edit->setEchoMode(TQLineEdit::Password);
    layout->addWidget(row->edit, 1);
    root->addLayout(layout);
}

void VpnConnectionEditorDialog::setRowShown(const EditorRow &row, bool shown)
{
    if (row.label)
        row.label->setShown(shown);
    if (row.edit)
        row.edit->setShown(shown);
}

VpnConnectionEditorDialog::VpnConnectionEditorDialog(NmClient *client,
                                                     const TQString &connectionPath,
                                                     TQWidget *parent)
    : TQDialog(parent, "vpn_connection_editor", true)
    , m_client(client)
    , m_connectionPath(connectionPath)
    , m_isNew(false)
    , m_newConnection(0)
    , m_kind(VpnKindGeneric)
    , m_nameEdit(0)
    , m_typeLabel(0)
    , m_autoconnectCheck(0)
    , m_passwordHint(0)
    , m_ipsecCheck(0)
    , m_ipsecGroup(0)
    , m_ipsecPskEdit(0)
    , m_wireguardHint(0)
{
    buildUi(i18n("Edit VPN Connection"));
    loadFromConnection();
    resize(520, m_kind == VpnKindWireGuard ? 520 : 460);
}

VpnConnectionEditorDialog::VpnConnectionEditorDialog(NmClient *client,
                                                     NMConnection *newConnection,
                                                     TQWidget *parent)
    : TQDialog(parent, "vpn_connection_editor", true)
    , m_client(client)
    , m_connectionPath(TQString::null)
    , m_isNew(true)
    , m_newConnection(newConnection)
    , m_kind(VpnKindGeneric)
    , m_nameEdit(0)
    , m_typeLabel(0)
    , m_autoconnectCheck(0)
    , m_passwordHint(0)
    , m_ipsecCheck(0)
    , m_ipsecGroup(0)
    , m_ipsecPskEdit(0)
    , m_wireguardHint(0)
{
    buildUi(i18n("New VPN Connection"));
    if (m_newConnection)
        loadFromConnectionObject(m_newConnection);
    resize(520, m_kind == VpnKindWireGuard ? 520 : 460);
}

VpnConnectionEditorDialog::~VpnConnectionEditorDialog()
{
    if (m_newConnection)
        g_object_unref(m_newConnection);
}

void VpnConnectionEditorDialog::buildUi(const TQString &caption)
{
    setCaption(caption);
    NmIcons::applyDialogIcon(this, NmIcons::vpnActiveIcon());

    TQVBoxLayout *root = new TQVBoxLayout(this, 12, 6);

    TQHBoxLayout *nameRow = new TQHBoxLayout();
    nameRow->setSpacing(6);
    nameRow->addWidget(new TQLabel(i18n("Connection name:"), this));
    m_nameEdit = new TQLineEdit(this);
    nameRow->addWidget(m_nameEdit, 1);
    root->addLayout(nameRow);

    TQHBoxLayout *typeRow = new TQHBoxLayout();
    typeRow->setSpacing(6);
    typeRow->addWidget(new TQLabel(i18n("VPN type:"), this));
    m_typeLabel = new TQLabel(this);
    m_typeLabel->setAlignment(TQt::AlignVCenter | TQt::AlignLeft);
    typeRow->addWidget(m_typeLabel, 1);
    root->addLayout(typeRow);

    m_autoconnectCheck = new TQCheckBox(i18n("Connect automatically"), this);
    root->addWidget(m_autoconnectCheck);

    addEditorRow(this, root, i18n("Server:"), &m_serverRow);
    addEditorRow(this, root, i18n("Username:"), &m_userRow);
    addEditorRow(this, root, i18n("Password:"), &m_passwordRow, true);

    m_passwordHint = new TQLabel(
        i18n("Leave password empty to keep the current value."), this);
    root->addWidget(m_passwordHint);

    addEditorRow(this, root, i18n("Group:"), &m_groupRow);
    addEditorRow(this, root, i18n("CA certificate:"), &m_caRow);
    addEditorRow(this, root, i18n("User certificate:"), &m_certRow);
    addEditorRow(this, root, i18n("Private key file:"), &m_keyRow);

    m_ipsecCheck = new TQCheckBox(i18n("Use IPsec"), this);
    root->addWidget(m_ipsecCheck);

    m_ipsecGroup = new TQGroupBox(this);
    TQVBoxLayout *ipsecLayout = new TQVBoxLayout(m_ipsecGroup, 8, 6);
    TQHBoxLayout *pskRow = new TQHBoxLayout();
    pskRow->setSpacing(6);
    pskRow->addWidget(new TQLabel(i18n("IPsec pre-shared key:"), m_ipsecGroup));
    m_ipsecPskEdit = new TQLineEdit(m_ipsecGroup);
    m_ipsecPskEdit->setEchoMode(TQLineEdit::Password);
    pskRow->addWidget(m_ipsecPskEdit, 1);
    ipsecLayout->addLayout(pskRow);
    ipsecLayout->addWidget(new TQLabel(
        i18n("Leave empty to keep the current IPsec key."), m_ipsecGroup));
    root->addWidget(m_ipsecGroup);
    connect(m_ipsecCheck, TQT_SIGNAL(toggled(bool)),
            this, TQT_SLOT(onIpsecToggled(bool)));

    addEditorRow(this, root, i18n("Private key:"), &m_privateKeyRow, true);
    addEditorRow(this, root, i18n("Peer public key:"), &m_publicKeyRow);
    addEditorRow(this, root, i18n("Endpoint:"), &m_endpointRow);
    addEditorRow(this, root, i18n("Allowed IPs:"), &m_allowedIpsRow);
    addEditorRow(this, root, i18n("Persistent keepalive (s):"), &m_keepaliveRow);

    m_wireguardHint = new TQLabel(
        i18n("WireGuard: endpoint is host:port. Allowed IPs use semicolon separators "
             "(e.g. 0.0.0.0/0)."), this);
    root->addWidget(m_wireguardHint);

    TQHBoxLayout *buttons = new TQHBoxLayout();
    buttons->setSpacing(6);
    buttons->addStretch(1);
    TQPushButton *cancelBtn = new TQPushButton(i18n("Cancel"), this);
    TQPushButton *okBtn = new TQPushButton(i18n("Save"), this);
    okBtn->setDefault(true);
    buttons->addWidget(cancelBtn);
    buttons->addWidget(okBtn);
    root->addLayout(buttons);

    connect(okBtn, TQT_SIGNAL(clicked()), this, TQT_SLOT(onAccept()));
    connect(cancelBtn, TQT_SIGNAL(clicked()), this, TQT_SLOT(reject()));
}

VpnConnectionEditorDialog::VpnEditorKind
VpnConnectionEditorDialog::detectKind(const char *service, NMConnection *conn) const
{
    if (conn && connectionWireguardSetting(conn))
        return VpnKindWireGuard;

    if (!service || !service[0])
        return VpnKindGeneric;

    if (strstr(service, "openvpn"))
        return VpnKindOpenVpn;
    if (strstr(service, "vpnc"))
        return VpnKindVpnc;
    if (strstr(service, "openconnect"))
        return VpnKindOpenConnect;
    if (strstr(service, "l2tp"))
        return VpnKindL2tp;
    if (strstr(service, "pptp"))
        return VpnKindPptp;
    if (strstr(service, "wireguard"))
        return VpnKindWireGuard;

    return VpnKindGeneric;
}

void VpnConnectionEditorDialog::applyFieldVisibility()
{
    bool generic = (m_kind == VpnKindGeneric);
    bool wireguard = (m_kind == VpnKindWireGuard);
    bool openvpn = (m_kind == VpnKindOpenVpn);
    bool vpnc = (m_kind == VpnKindVpnc);
    bool l2tp = (m_kind == VpnKindL2tp);
    bool showUserPass = !wireguard;

    setRowShown(m_serverRow, !wireguard);
    setRowShown(m_userRow, showUserPass);
    setRowShown(m_passwordRow, showUserPass);
    m_passwordHint->setShown(showUserPass && !m_isNew);
    setRowShown(m_groupRow, vpnc);
    setRowShown(m_caRow, openvpn);
    setRowShown(m_certRow, openvpn);
    setRowShown(m_keyRow, openvpn);
    if (m_ipsecCheck)
        m_ipsecCheck->setShown(l2tp);
    if (m_ipsecGroup)
        m_ipsecGroup->setShown(l2tp);

    setRowShown(m_privateKeyRow, wireguard);
    setRowShown(m_publicKeyRow, wireguard);
    setRowShown(m_endpointRow, wireguard);
    setRowShown(m_allowedIpsRow, wireguard);
    setRowShown(m_keepaliveRow, wireguard);
    if (m_wireguardHint)
        m_wireguardHint->setShown(wireguard);

    if (generic) {
        setRowShown(m_groupRow, false);
        setRowShown(m_caRow, false);
        setRowShown(m_certRow, false);
        setRowShown(m_keyRow, false);
        if (m_ipsecCheck)
            m_ipsecCheck->hide();
        if (m_ipsecGroup)
            m_ipsecGroup->hide();
        setRowShown(m_privateKeyRow, false);
        setRowShown(m_publicKeyRow, false);
        setRowShown(m_endpointRow, false);
        setRowShown(m_allowedIpsRow, false);
        setRowShown(m_keepaliveRow, false);
        if (m_wireguardHint)
            m_wireguardHint->hide();
    }

    if (l2tp)
        onIpsecToggled(m_ipsecCheck->isChecked());
}

void VpnConnectionEditorDialog::onIpsecToggled(bool enabled)
{
    if (m_ipsecGroup)
        m_ipsecGroup->setEnabled(enabled);
    if (m_ipsecPskEdit)
        m_ipsecPskEdit->setEnabled(enabled);
}

NMConnection *VpnConnectionEditorDialog::editableConnection()
{
    NMClient *client = m_client ? m_client->nmClient() : 0;
    NMRemoteConnection *remote;

    if (m_newConnection)
        return m_newConnection;

    if (!client || m_connectionPath.isEmpty())
        return 0;

    remote = nm_client_get_connection_by_path(client, m_connectionPath.utf8().data());
    if (!remote)
        return 0;

    return NM_CONNECTION(remote);
}

void VpnConnectionEditorDialog::loadFromConnection()
{
    NMConnection *conn = editableConnection();
    if (conn)
        loadFromConnectionObject(conn);
}

void VpnConnectionEditorDialog::loadFromConnectionObject(NMConnection *conn)
{
    NMSettingConnection *s_con;
    NMSettingVpn *s_vpn;
    NMSettingWireGuard *s_wg;
    const char *service;
    static const char *serverKeys[] = {
        "remote", "gateway", "ip", "server", "host", 0
    };
    static const char *userKeys[] = { "username", "user", 0 };
    static const char *passwordKeys[] = {
        "password", "passwd", "secret", "ipsec-secret", "ipsec-psk", 0
    };

    if (!conn)
        return;

    s_con = nm_connection_get_setting_connection(conn);
    s_vpn = nm_connection_get_setting_vpn(conn);
    s_wg = connectionWireguardSetting(conn);
    if (!s_con)
        return;

    if (nm_setting_connection_get_id(s_con))
        m_nameEdit->setText(TQString::fromUtf8(nm_setting_connection_get_id(s_con)));

    m_autoconnectCheck->setChecked(
        nm_setting_connection_get_autoconnect(s_con) ? true : false);

    service = s_vpn ? nm_setting_vpn_get_service_type(s_vpn) : 0;
    m_kind = detectKind(service, conn);
    m_typeLabel->setText(vpnServiceLabel(
        m_kind == VpnKindWireGuard ? "wireguard" : service));
    applyFieldVisibility();

    if (m_kind == VpnKindWireGuard && s_wg) {
        NMWireGuardPeer *peer = 0;

        if (nm_setting_wireguard_get_peers_len(s_wg) > 0)
            peer = nm_setting_wireguard_get_peer(s_wg, 0);

        if (peer) {
            const char *publicKey = nm_wireguard_peer_get_public_key(peer);
            const char *endpoint = nm_wireguard_peer_get_endpoint(peer);

            if (publicKey && publicKey[0])
                m_publicKeyRow.edit->setText(TQString::fromUtf8(publicKey));
            if (endpoint && endpoint[0])
                m_endpointRow.edit->setText(TQString::fromUtf8(endpoint));

            m_allowedIpsRow.edit->setText(allowedIpsToEdit(peer));

            guint16 keepalive = nm_wireguard_peer_get_persistent_keepalive(peer);
            if (keepalive > 0)
                m_keepaliveRow.edit->setText(TQString::number(keepalive));
        }
        return;
    }

    if (!s_vpn)
        return;

    m_serverKey = findVpnDataKey(s_vpn, serverKeys);
    if (m_serverKey.isEmpty() && service)
        m_serverKey = TQString::fromUtf8(defaultServerKey(service));
    m_serverRow.edit->setText(findVpnDataValue(s_vpn, serverKeys));

    if (nm_setting_vpn_get_user_name(s_vpn)) {
        m_userRow.edit->setText(TQString::fromUtf8(nm_setting_vpn_get_user_name(s_vpn)));
    } else {
        m_userRow.edit->setText(findVpnDataValue(s_vpn, userKeys));
    }

    m_passwordKey = findVpnSecretKey(s_vpn, passwordKeys);

    if (m_kind == VpnKindVpnc)
        m_groupRow.edit->setText(vpnDataValue(s_vpn, "group"));

    if (m_kind == VpnKindOpenVpn) {
        m_caRow.edit->setText(vpnDataValue(s_vpn, "ca"));
        m_certRow.edit->setText(vpnDataValue(s_vpn, "cert"));
        m_keyRow.edit->setText(vpnDataValue(s_vpn, "key"));
    }

    if (m_kind == VpnKindL2tp) {
        TQString ipsecEnabled = vpnDataValue(s_vpn, "ipsec-enabled");
        bool enabled = ipsecEnabled.isEmpty()
            || ipsecEnabled.lower() == "yes"
            || ipsecEnabled == "1"
            || ipsecEnabled.lower() == "true";
        m_ipsecCheck->setChecked(enabled);
        onIpsecToggled(enabled);
    }
}

bool VpnConnectionEditorDialog::saveProfile(TQString *errorOut)
{
    NMClient *client = m_client ? m_client->nmClient() : 0;
    NMConnection *conn;
    NMSettingConnection *s_con;
    NMSettingVpn *s_vpn;
    NMSettingWireGuard *s_wg;
    TQString name;

    if (!client) {
        if (errorOut)
            *errorOut = i18n("NetworkManager is not available.");
        return false;
    }

    conn = editableConnection();
    s_con = conn ? nm_connection_get_setting_connection(conn) : 0;
    s_vpn = conn ? nm_connection_get_setting_vpn(conn) : 0;
    s_wg = conn ? connectionWireguardSetting(conn) : 0;
    if (!conn || !s_con) {
        if (errorOut)
            *errorOut = i18n("Invalid VPN connection.");
        return false;
    }

    name = m_nameEdit->text().stripWhiteSpace();
    if (name.isEmpty()) {
        if (errorOut)
            *errorOut = i18n("Please enter a connection name.");
        return false;
    }

    g_object_set(G_OBJECT(s_con),
                 NM_SETTING_CONNECTION_ID, name.utf8().data(),
                 NM_SETTING_CONNECTION_AUTOCONNECT,
                 m_autoconnectCheck->isChecked() ? TRUE : FALSE,
                 NULL);

    if (m_kind == VpnKindWireGuard) {
        TQString privateKey = m_privateKeyRow.edit->text().stripWhiteSpace();
        TQString publicKey = m_publicKeyRow.edit->text().stripWhiteSpace();
        TQString endpoint = m_endpointRow.edit->text().stripWhiteSpace();
        TQString allowedIps = m_allowedIpsRow.edit->text().stripWhiteSpace();
        TQString keepaliveText = m_keepaliveRow.edit->text().stripWhiteSpace();
        NMWireGuardPeer *peer;
        guint16 keepalive = 0;

        if (!s_wg) {
            if (errorOut)
                *errorOut = i18n("Invalid WireGuard connection.");
            return false;
        }

        if (publicKey.isEmpty()) {
            if (errorOut)
                *errorOut = i18n("Please enter the peer public key.");
            return false;
        }
        if (endpoint.isEmpty()) {
            if (errorOut)
                *errorOut = i18n("Please enter the WireGuard endpoint.");
            return false;
        }
        if (allowedIps.isEmpty()) {
            if (errorOut)
                *errorOut = i18n("Please enter at least one allowed IP range.");
            return false;
        }
        if (m_isNew && privateKey.isEmpty()) {
            if (errorOut)
                *errorOut = i18n("Please enter the WireGuard private key.");
            return false;
        }

        if (!keepaliveText.isEmpty()) {
            bool ok = false;
            int value = keepaliveText.toInt(&ok);
            if (!ok || value < 0 || value > 65535) {
                if (errorOut)
                    *errorOut = i18n("Persistent keepalive must be between 0 and 65535.");
                return false;
            }
            keepalive = (guint16) value;
        }

        if (!privateKey.isEmpty()) {
            g_object_set(G_OBJECT(s_wg),
                         NM_SETTING_WIREGUARD_PRIVATE_KEY, privateKey.utf8().data(),
                         NM_SETTING_WIREGUARD_PRIVATE_KEY_FLAGS,
                         (guint) NM_SETTING_SECRET_FLAG_NONE,
                         NULL);
        }

        if (nm_setting_wireguard_get_peers_len(s_wg) > 0)
            peer = nm_setting_wireguard_get_peer(s_wg, 0);
        else
            peer = nm_wireguard_peer_new();

        if (!nm_wireguard_peer_set_public_key(peer, publicKey.utf8().data(), FALSE)) {
            if (errorOut)
                *errorOut = i18n("Invalid WireGuard public key.");
            if (nm_setting_wireguard_get_peers_len(s_wg) == 0)
                nm_wireguard_peer_unref(peer);
            return false;
        }

        if (!nm_wireguard_peer_set_endpoint(peer, endpoint.utf8().data(), FALSE)) {
            if (errorOut)
                *errorOut = i18n("Invalid WireGuard endpoint.");
            if (nm_setting_wireguard_get_peers_len(s_wg) == 0)
                nm_wireguard_peer_unref(peer);
            return false;
        }

        if (!applyAllowedIps(peer, allowedIps)) {
            if (errorOut)
                *errorOut = i18n("Invalid allowed IP range.");
            if (nm_setting_wireguard_get_peers_len(s_wg) == 0)
                nm_wireguard_peer_unref(peer);
            return false;
        }

        nm_wireguard_peer_set_persistent_keepalive(peer, keepalive);

        if (nm_setting_wireguard_get_peers_len(s_wg) > 0)
            nm_setting_wireguard_set_peer(s_wg, peer, 0);
        else
            nm_setting_wireguard_append_peer(s_wg, peer);

        if (m_isNew) {
            if (!addConnectionProfile(client, conn, &m_connectionPath, errorOut))
                return false;
            m_isNew = false;
            g_object_unref(m_newConnection);
            m_newConnection = 0;
            return true;
        }

        return commitConnectionProfile(NM_REMOTE_CONNECTION(conn), errorOut);
    }

    if (!s_vpn) {
        if (errorOut)
            *errorOut = i18n("Invalid VPN connection.");
        return false;
    }

    TQString server = m_serverRow.edit->text().stripWhiteSpace();
    TQString user = m_userRow.edit->text().stripWhiteSpace();
    TQString password = m_passwordRow.edit->text();

    if (server.isEmpty()) {
        if (errorOut)
            *errorOut = i18n("Please enter the VPN server.");
        return false;
    }

    if (m_isNew && password.isEmpty()) {
        if (errorOut)
            *errorOut = i18n("Please enter the VPN password.");
        return false;
    }

    if (!m_serverKey.isEmpty())
        setVpnDataItem(s_vpn, m_serverKey.utf8().data(), server);

    g_object_set(G_OBJECT(s_vpn),
                 NM_SETTING_VPN_USER_NAME, user.utf8().data(),
                 NULL);

    if (m_kind == VpnKindVpnc)
        setVpnDataItem(s_vpn, "group", m_groupRow.edit->text().stripWhiteSpace());

    if (m_kind == VpnKindOpenVpn) {
        setVpnDataItem(s_vpn, "ca", m_caRow.edit->text().stripWhiteSpace());
        setVpnDataItem(s_vpn, "cert", m_certRow.edit->text().stripWhiteSpace());
        setVpnDataItem(s_vpn, "key", m_keyRow.edit->text().stripWhiteSpace());
    }

    if (m_kind == VpnKindL2tp) {
        if (m_ipsecCheck->isChecked()) {
            setVpnDataItem(s_vpn, "ipsec-enabled", "yes");
            TQString psk = m_ipsecPskEdit->text();
            if (!psk.isEmpty())
                setVpnSecret(s_vpn, "ipsec-psk", psk);
        } else {
            setVpnDataItem(s_vpn, "ipsec-enabled", "no");
        }
    }

    if (!password.isEmpty() && !m_passwordKey.isEmpty())
        setVpnSecret(s_vpn, m_passwordKey.utf8().data(), password);

    if (m_isNew) {
        if (!addConnectionProfile(client, conn, &m_connectionPath, errorOut))
            return false;
        m_isNew = false;
        g_object_unref(m_newConnection);
        m_newConnection = 0;
        return true;
    }

    return commitConnectionProfile(NM_REMOTE_CONNECTION(conn), errorOut);
}

void VpnConnectionEditorDialog::showEvent(TQShowEvent *event)
{
    TQDialog::showEvent(event);
    centerEditorDialog(this);
}

void VpnConnectionEditorDialog::onAccept()
{
    TQString error;
    TQString title = m_isNew
        ? i18n("New VPN Connection")
        : i18n("Edit VPN Connection");
    if (!saveProfile(&error)) {
        KMessageBox::error(this, error, title);
        return;
    }
    accept();
}

#include "vpnconnectioneditordialog.moc"
