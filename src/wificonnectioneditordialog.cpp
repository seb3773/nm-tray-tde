#include "wificonnectioneditordialog.h"
#include "connectioneditorutil.h"
#include "ipv4config.h"
#include "icons.h"
#include "wifiutil.h"
#include "nm/nmclient.h"
#include "nm/nmeventpump.h"
#include "nm/glib_compat.h"

#include <tdelocale.h>
#include <tqlabel.h>
#include <tqlineedit.h>
#include <tqcombobox.h>
#include <tqpushbutton.h>
#include <tqcheckbox.h>
#include <tqgroupbox.h>
#include <tqlayout.h>
#include <tdemessagebox.h>
#include <tqevent.h>

WifiConnectionEditorDialog::WifiConnectionEditorDialog(NmClient *client,
                                                       const TQString &connectionPath,
                                                       TQWidget *parent)
    : TQDialog(parent, "wifi_connection_editor", true)
    , m_client(client)
    , m_connectionPath(connectionPath)
    , m_isNew(false)
    , m_newConnection(0)
    , m_loadedSecurityType(WifiSecNone)
    , m_nameEdit(0)
    , m_ssidEdit(0)
    , m_securityCombo(0)
    , m_usernameLabel(0)
    , m_usernameEdit(0)
    , m_secretLabel(0)
    , m_secretEdit(0)
    , m_autoconnectCheck(0)
    , m_hiddenCheck(0)
    , m_mtuEdit(0)
    , m_manualIpCheck(0)
    , m_manualIpGroup(0)
    , m_ipEdit(0)
    , m_netmaskEdit(0)
    , m_gatewayEdit(0)
    , m_dnsEdit(0)
    , m_dnsSearchEdit(0)
{
    buildUi(i18n("Edit Wi-Fi Connection"));
    populateSecurityCombo();
    loadFromConnection();
    if (m_securityCombo->count() > 0)
        onSecurityChanged(m_securityCombo->currentItem());
    onManualIpToggled(m_manualIpCheck->isChecked());
    resize(520, 560);
}

WifiConnectionEditorDialog::WifiConnectionEditorDialog(NmClient *client,
                                                       NMConnection *newConnection,
                                                       TQWidget *parent)
    : TQDialog(parent, "wifi_connection_editor", true)
    , m_client(client)
    , m_connectionPath(TQString::null)
    , m_isNew(true)
    , m_newConnection(newConnection)
    , m_loadedSecurityType(WifiSecNone)
    , m_nameEdit(0)
    , m_ssidEdit(0)
    , m_securityCombo(0)
    , m_usernameLabel(0)
    , m_usernameEdit(0)
    , m_secretLabel(0)
    , m_secretEdit(0)
    , m_autoconnectCheck(0)
    , m_hiddenCheck(0)
    , m_mtuEdit(0)
    , m_manualIpCheck(0)
    , m_manualIpGroup(0)
    , m_ipEdit(0)
    , m_netmaskEdit(0)
    , m_gatewayEdit(0)
    , m_dnsEdit(0)
    , m_dnsSearchEdit(0)
{
    buildUi(i18n("New Wi-Fi Connection"));
    populateSecurityCombo();
    if (m_newConnection)
        loadFromConnectionObject(m_newConnection);
    if (m_securityCombo->count() > 0)
        onSecurityChanged(m_securityCombo->currentItem());
    onManualIpToggled(m_manualIpCheck->isChecked());
    resize(520, 560);
}

WifiConnectionEditorDialog::~WifiConnectionEditorDialog()
{
    if (m_newConnection)
        g_object_unref(m_newConnection);
}

void WifiConnectionEditorDialog::buildUi(const TQString &caption)
{
    setCaption(caption);
    NmIcons::applyDialogIcon(this, NmIcons::wirelessIcon());

    TQVBoxLayout *root = new TQVBoxLayout(this, 12, 6);

    TQHBoxLayout *nameRow = new TQHBoxLayout();
    nameRow->setSpacing(6);
    nameRow->addWidget(new TQLabel(i18n("Connection name:"), this));
    m_nameEdit = new TQLineEdit(this);
    nameRow->addWidget(m_nameEdit, 1);
    root->addLayout(nameRow);

    TQHBoxLayout *ssidRow = new TQHBoxLayout();
    ssidRow->setSpacing(6);
    ssidRow->addWidget(new TQLabel(i18n("Network name (SSID):"), this));
    m_ssidEdit = new TQLineEdit(this);
    ssidRow->addWidget(m_ssidEdit, 1);
    root->addLayout(ssidRow);

    TQHBoxLayout *secRow = new TQHBoxLayout();
    secRow->setSpacing(6);
    secRow->addWidget(new TQLabel(i18n("Security:"), this));
    m_securityCombo = new TQComboBox(this);
    secRow->addWidget(m_securityCombo, 1);
    root->addLayout(secRow);

    TQHBoxLayout *userRow = new TQHBoxLayout();
    userRow->setSpacing(6);
    m_usernameLabel = new TQLabel(i18n("Username:"), this);
    m_usernameEdit = new TQLineEdit(this);
    userRow->addWidget(m_usernameLabel);
    userRow->addWidget(m_usernameEdit, 1);
    root->addLayout(userRow);

    TQHBoxLayout *secretRow = new TQHBoxLayout();
    secretRow->setSpacing(6);
    m_secretLabel = new TQLabel(i18n("Password:"), this);
    m_secretEdit = new TQLineEdit(this);
    m_secretEdit->setEchoMode(TQLineEdit::Password);
    secretRow->addWidget(m_secretLabel);
    secretRow->addWidget(m_secretEdit, 1);
    root->addLayout(secretRow);

    m_autoconnectCheck = new TQCheckBox(i18n("Connect automatically"), this);
    root->addWidget(m_autoconnectCheck);

    m_hiddenCheck = new TQCheckBox(i18n("Hidden network"), this);
    root->addWidget(m_hiddenCheck);

    TQHBoxLayout *mtuRow = new TQHBoxLayout();
    mtuRow->setSpacing(6);
    mtuRow->addWidget(new TQLabel(i18n("MTU:"), this));
    m_mtuEdit = new TQLineEdit(this);
    mtuRow->addWidget(m_mtuEdit, 1);
    root->addLayout(mtuRow);
    root->addWidget(new TQLabel(
        i18n("Leave MTU empty to use the automatic value."), this));

    m_manualIpCheck = new TQCheckBox(i18n("Use manual IP configuration"), this);
    root->addWidget(m_manualIpCheck);

    m_manualIpGroup = new TQGroupBox(this);
    TQVBoxLayout *ipGroupLayout = new TQVBoxLayout(m_manualIpGroup, 8, 6);

    TQHBoxLayout *ipRow = new TQHBoxLayout();
    ipRow->setSpacing(6);
    ipRow->addWidget(new TQLabel(i18n("IP address:"), m_manualIpGroup));
    m_ipEdit = new TQLineEdit(m_manualIpGroup);
    ipRow->addWidget(m_ipEdit, 1);
    ipGroupLayout->addLayout(ipRow);

    TQHBoxLayout *maskRow = new TQHBoxLayout();
    maskRow->setSpacing(6);
    maskRow->addWidget(new TQLabel(i18n("Netmask:"), m_manualIpGroup));
    m_netmaskEdit = new TQLineEdit(m_manualIpGroup);
    maskRow->addWidget(m_netmaskEdit, 1);
    ipGroupLayout->addLayout(maskRow);

    TQHBoxLayout *gwRow = new TQHBoxLayout();
    gwRow->setSpacing(6);
    gwRow->addWidget(new TQLabel(i18n("Gateway:"), m_manualIpGroup));
    m_gatewayEdit = new TQLineEdit(m_manualIpGroup);
    gwRow->addWidget(m_gatewayEdit, 1);
    ipGroupLayout->addLayout(gwRow);

    TQHBoxLayout *dnsRow = new TQHBoxLayout();
    dnsRow->setSpacing(6);
    dnsRow->addWidget(new TQLabel(i18n("DNS addresses:"), m_manualIpGroup));
    m_dnsEdit = new TQLineEdit(m_manualIpGroup);
    dnsRow->addWidget(m_dnsEdit, 1);
    ipGroupLayout->addLayout(dnsRow);

    TQHBoxLayout *searchRow = new TQHBoxLayout();
    searchRow->setSpacing(6);
    searchRow->addWidget(new TQLabel(i18n("DNS search:"), m_manualIpGroup));
    m_dnsSearchEdit = new TQLineEdit(m_manualIpGroup);
    searchRow->addWidget(m_dnsSearchEdit, 1);
    ipGroupLayout->addLayout(searchRow);

    root->addWidget(m_manualIpGroup);
    root->addWidget(new TQLabel(
        i18n("DNS fields accept multiple values separated by spaces."), this));

    TQHBoxLayout *buttons = new TQHBoxLayout();
    buttons->setSpacing(6);
    buttons->addStretch(1);
    TQPushButton *cancelBtn = new TQPushButton(i18n("Cancel"), this);
    TQPushButton *okBtn = new TQPushButton(i18n("Save"), this);
    okBtn->setDefault(true);
    buttons->addWidget(cancelBtn);
    buttons->addWidget(okBtn);
    root->addLayout(buttons);

    connect(m_securityCombo, TQT_SIGNAL(activated(int)),
            this, TQT_SLOT(onSecurityChanged(int)));
    connect(m_manualIpCheck, TQT_SIGNAL(toggled(bool)),
            this, TQT_SLOT(onManualIpToggled(bool)));
    connect(okBtn, TQT_SIGNAL(clicked()), this, TQT_SLOT(onAccept()));
    connect(cancelBtn, TQT_SIGNAL(clicked()), this, TQT_SLOT(reject()));
}

NMConnection *WifiConnectionEditorDialog::editableConnection()
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

void WifiConnectionEditorDialog::populateSecurityCombo()
{
    NMClient *client = m_client ? m_client->nmClient() : 0;

    m_securityCombo->clear();
    m_securityTypes.clear();

    for (int i = WifiSecNone; i < WifiSec_Count; ++i) {
        WifiSecurityType type = (WifiSecurityType) i;
        if (!wifiSecurityAvailableOnClient(type, client))
            continue;
        m_securityTypes.append(type);
        m_securityCombo->insertItem(wifiSecurityLabel(type));
    }

    if (m_securityCombo->count() == 0) {
        m_securityTypes.append(WifiSecNone);
        m_securityCombo->insertItem(wifiSecurityLabel(WifiSecNone));
    }
}

void WifiConnectionEditorDialog::loadFromConnection()
{
    NMConnection *conn = editableConnection();
    if (conn)
        loadFromConnectionObject(conn);
}

void WifiConnectionEditorDialog::loadFromConnectionObject(NMConnection *conn)
{
    NMSettingConnection *s_con;
    NMSettingWireless *s_wifi;
    GBytes *ssidBytes;
    Ipv4EditorState ipState;
    guint32 mtu;

    if (!conn)
        return;

    s_con = nm_connection_get_setting_connection(conn);
    s_wifi = nm_connection_get_setting_wireless(conn);
    if (!s_con || !s_wifi)
        return;

    if (nm_setting_connection_get_id(s_con))
        m_nameEdit->setText(TQString::fromUtf8(nm_setting_connection_get_id(s_con)));

    ssidBytes = nm_setting_wireless_get_ssid(s_wifi);
    m_ssidEdit->setText(ssidToString(ssidBytes));

    m_autoconnectCheck->setChecked(
        nm_setting_connection_get_autoconnect(s_con) ? true : false);
    m_hiddenCheck->setChecked(
        nm_setting_wireless_get_hidden(s_wifi) ? true : false);

    mtu = nm_setting_wireless_get_mtu(s_wifi);
    if (mtu > 0)
        m_mtuEdit->setText(TQString::number(mtu));

    ipv4EditorLoad(conn, &ipState);
    m_manualIpCheck->setChecked(ipState.manual);
    m_ipEdit->setText(ipState.address);
    m_netmaskEdit->setText(ipState.netmask);
    m_gatewayEdit->setText(ipState.gateway);
    m_dnsEdit->setText(ipState.dns);
    m_dnsSearchEdit->setText(ipState.dnsSearch);

    m_loadedSecurityType = wifiSecurityDetect(conn);
    m_usernameEdit->setText(wifiSecurityReadIdentity(conn));

    for (uint i = 0; i < m_securityTypes.size(); ++i) {
        if (m_securityTypes[i] == m_loadedSecurityType) {
            m_securityCombo->setCurrentItem((int) i);
            break;
        }
    }
}

void WifiConnectionEditorDialog::onManualIpToggled(bool enabled)
{
    if (m_manualIpGroup)
        m_manualIpGroup->setEnabled(enabled);
    if (m_ipEdit)
        m_ipEdit->setEnabled(enabled);
    if (m_netmaskEdit)
        m_netmaskEdit->setEnabled(enabled);
    if (m_gatewayEdit)
        m_gatewayEdit->setEnabled(enabled);
    if (m_dnsEdit)
        m_dnsEdit->setEnabled(enabled);
    if (m_dnsSearchEdit)
        m_dnsSearchEdit->setEnabled(enabled);
}

WifiSecurityType WifiConnectionEditorDialog::currentSecurityType() const
{
    int index = m_securityCombo->currentItem();
    if (index < 0 || index >= (int) m_securityTypes.size())
        return WifiSecNone;
    return m_securityTypes[(uint) index];
}

void WifiConnectionEditorDialog::updateCredentialFields()
{
    WifiSecurityType type = currentSecurityType();
    bool needIdentity = wifiSecurityNeedsIdentity(type);
    bool needSecret = wifiSecurityNeedsSecret(type);

    if (needIdentity) {
        m_usernameLabel->show();
        m_usernameEdit->show();
    } else {
        m_usernameLabel->hide();
        m_usernameEdit->hide();
        m_usernameEdit->clear();
    }

    if (needSecret) {
        m_secretLabel->show();
        m_secretEdit->show();
        m_secretLabel->setText(m_isNew
            ? i18n("Password:")
            : i18n("Password (leave blank to keep):"));
    } else {
        m_secretLabel->hide();
        m_secretEdit->hide();
        m_secretEdit->clear();
    }
}

void WifiConnectionEditorDialog::showEvent(TQShowEvent *event)
{
    TQDialog::showEvent(event);
    centerEditorDialog(this);
}

void WifiConnectionEditorDialog::onSecurityChanged(int index)
{
    (void) index;
    updateCredentialFields();
}

bool WifiConnectionEditorDialog::saveProfile(TQString *errorOut)
{
    NMClient *client = m_client ? m_client->nmClient() : 0;
    NMConnection *conn;
    NMSettingConnection *s_con;
    NMSettingWireless *s_wifi;
    WifiSecurityType newType;
    WifiSecurityType oldType;
    Ipv4EditorState ipState;
    TQString name;
    TQString ssid;
    TQString identity;
    TQString secret;
    TQString mtuText;
    bool secretProvided;
    GBytes *ssidBytes;
    GError *error = 0;

    if (!client) {
        if (errorOut)
            *errorOut = i18n("NetworkManager is not available.");
        return false;
    }

    conn = editableConnection();
    s_con = conn ? nm_connection_get_setting_connection(conn) : 0;
    s_wifi = conn ? nm_connection_get_setting_wireless(conn) : 0;
    if (!conn || !s_con || !s_wifi) {
        if (errorOut)
            *errorOut = i18n("Invalid Wi-Fi connection.");
        return false;
    }

    name = m_nameEdit->text().stripWhiteSpace();
    ssid = m_ssidEdit->text().stripWhiteSpace();
    if (name.isEmpty()) {
        if (errorOut)
            *errorOut = i18n("Please enter a connection name.");
        return false;
    }
    if (ssid.isEmpty()) {
        if (errorOut)
            *errorOut = i18n("Please enter the network name (SSID).");
        return false;
    }

    mtuText = m_mtuEdit->text().stripWhiteSpace();
    if (mtuText.isEmpty()) {
        g_object_set(G_OBJECT(s_wifi), NM_SETTING_WIRELESS_MTU, (guint32) 0, NULL);
    } else {
        bool ok = false;
        uint mtu = mtuText.toUInt(&ok);
        if (!ok || mtu < 68 || mtu > 9000) {
            if (errorOut)
                *errorOut = i18n("Please enter a valid MTU (68-9000).");
            return false;
        }
        g_object_set(G_OBJECT(s_wifi), NM_SETTING_WIRELESS_MTU, (guint32) mtu, NULL);
    }

    newType = currentSecurityType();
    oldType = m_loadedSecurityType;
    identity = m_usernameEdit->text().stripWhiteSpace();
    secret = m_secretEdit->text();
    secretProvided = !secret.isEmpty();

    g_object_set(G_OBJECT(s_con),
                 NM_SETTING_CONNECTION_ID, name.utf8().data(),
                 NM_SETTING_CONNECTION_AUTOCONNECT,
                 m_autoconnectCheck->isChecked() ? TRUE : FALSE,
                 NULL);

    ssidBytes = g_bytes_new(ssid.utf8().data(), ssid.utf8().length());
    g_object_set(G_OBJECT(s_wifi),
                 NM_SETTING_WIRELESS_SSID, ssidBytes,
                 NM_SETTING_WIRELESS_HIDDEN,
                 m_hiddenCheck->isChecked() ? TRUE : FALSE,
                 NULL);
    g_bytes_unref(ssidBytes);

    if (newType != oldType || m_isNew) {
        wifiSecurityClear(conn);
        if (wifiSecurityNeedsSecret(newType) && !secretProvided) {
            if (errorOut)
                *errorOut = i18n("Please enter the required credentials.");
            return false;
        }
        if (!wifiSecurityApply(conn, newType, identity, secret, errorOut))
            return false;
    } else if (secretProvided) {
        wifiSecurityClear(conn);
        if (!wifiSecurityApply(conn, newType, identity, secret, errorOut))
            return false;
    } else if (wifiSecurityNeedsIdentity(newType)) {
        if (identity.isEmpty()) {
            if (errorOut)
                *errorOut = i18n("Please enter a username.");
            return false;
        }
        NMSetting8021x *s_8021x = nm_connection_get_setting_802_1x(conn);
        if (s_8021x) {
            g_object_set(G_OBJECT(s_8021x),
                         NM_SETTING_802_1X_IDENTITY, identity.utf8().data(),
                         NULL);
        }
    }

    ipState.manual = m_manualIpCheck->isChecked();
    ipState.address = m_ipEdit->text();
    ipState.netmask = m_netmaskEdit->text();
    ipState.gateway = m_gatewayEdit->text();
    ipState.dns = m_dnsEdit->text();
    ipState.dnsSearch = m_dnsSearchEdit->text();

    if (!ipv4EditorApply(conn, ipState, errorOut))
        return false;

    if (m_isNew) {
        if (!addConnectionProfile(client, conn, &m_connectionPath, errorOut))
            return false;
        m_isNew = false;
        g_object_unref(m_newConnection);
        m_newConnection = 0;
        return true;
    }

    if (!nm_remote_connection_commit_changes(NM_REMOTE_CONNECTION(conn), TRUE, NULL, &error)) {
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

void WifiConnectionEditorDialog::onAccept()
{
    TQString error;
    TQString title = m_isNew
        ? i18n("New Wi-Fi Connection")
        : i18n("Edit Wi-Fi Connection");
    if (!saveProfile(&error)) {
        KMessageBox::error(this, error, title);
        return;
    }
    accept();
}

#include "wificonnectioneditordialog.moc"
