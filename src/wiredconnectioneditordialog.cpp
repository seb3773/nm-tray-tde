#include "wiredconnectioneditordialog.h"
#include "connectioneditorutil.h"
#include "ipv4config.h"
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

WiredConnectionEditorDialog::WiredConnectionEditorDialog(NmClient *client,
                                                         const TQString &connectionPath,
                                                         TQWidget *parent)
    : TQDialog(parent, "wired_connection_editor", true)
    , m_client(client)
    , m_connectionPath(connectionPath)
    , m_isNew(false)
    , m_newConnection(0)
    , m_nameEdit(0)
    , m_macEdit(0)
    , m_autoconnectCheck(0)
    , m_manualIpCheck(0)
    , m_manualIpGroup(0)
    , m_ipEdit(0)
    , m_netmaskEdit(0)
    , m_gatewayEdit(0)
    , m_dnsEdit(0)
    , m_dnsSearchEdit(0)
{
    buildUi(i18n("Edit Wired Connection"));
    loadFromConnection();
    onManualIpToggled(m_manualIpCheck->isChecked());
    resize(520, 420);
}

WiredConnectionEditorDialog::WiredConnectionEditorDialog(NmClient *client,
                                                         NMConnection *newConnection,
                                                         TQWidget *parent)
    : TQDialog(parent, "wired_connection_editor", true)
    , m_client(client)
    , m_connectionPath(TQString::null)
    , m_isNew(true)
    , m_newConnection(newConnection)
    , m_nameEdit(0)
    , m_macEdit(0)
    , m_autoconnectCheck(0)
    , m_manualIpCheck(0)
    , m_manualIpGroup(0)
    , m_ipEdit(0)
    , m_netmaskEdit(0)
    , m_gatewayEdit(0)
    , m_dnsEdit(0)
    , m_dnsSearchEdit(0)
{
    buildUi(i18n("New Wired Connection"));
    if (m_newConnection)
        loadFromConnectionObject(m_newConnection);
    onManualIpToggled(m_manualIpCheck->isChecked());
    resize(520, 420);
}

WiredConnectionEditorDialog::~WiredConnectionEditorDialog()
{
    if (m_newConnection)
        g_object_unref(m_newConnection);
}

void WiredConnectionEditorDialog::buildUi(const TQString &caption)
{
    setCaption(caption);
    NmIcons::applyDialogIcon(this, NmIcons::wiredIcon());

    TQVBoxLayout *root = new TQVBoxLayout(this, 12, 6);

    TQHBoxLayout *nameRow = new TQHBoxLayout();
    nameRow->setSpacing(6);
    nameRow->addWidget(new TQLabel(i18n("Connection name:"), this));
    m_nameEdit = new TQLineEdit(this);
    nameRow->addWidget(m_nameEdit, 1);
    root->addLayout(nameRow);

    m_autoconnectCheck = new TQCheckBox(i18n("Connect automatically"), this);
    root->addWidget(m_autoconnectCheck);

    TQHBoxLayout *macRow = new TQHBoxLayout();
    macRow->setSpacing(6);
    macRow->addWidget(new TQLabel(i18n("MAC address:"), this));
    m_macEdit = new TQLineEdit(this);
    macRow->addWidget(m_macEdit, 1);
    root->addLayout(macRow);
    root->addWidget(new TQLabel(
        i18n("Leave empty to use the device default MAC address."), this));

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

    connect(m_manualIpCheck, TQT_SIGNAL(toggled(bool)),
            this, TQT_SLOT(onManualIpToggled(bool)));
    connect(okBtn, TQT_SIGNAL(clicked()), this, TQT_SLOT(onAccept()));
    connect(cancelBtn, TQT_SIGNAL(clicked()), this, TQT_SLOT(reject()));
}

NMConnection *WiredConnectionEditorDialog::editableConnection()
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

void WiredConnectionEditorDialog::onManualIpToggled(bool enabled)
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

void WiredConnectionEditorDialog::loadFromConnection()
{
    NMConnection *conn = editableConnection();
    if (conn)
        loadFromConnectionObject(conn);
}

void WiredConnectionEditorDialog::loadFromConnectionObject(NMConnection *conn)
{
    NMSettingConnection *s_con;
    NMSettingWired *s_wired;
    Ipv4EditorState ipState;
    const char *mac;

    if (!conn)
        return;

    s_con = nm_connection_get_setting_connection(conn);
    s_wired = nm_connection_get_setting_wired(conn);
    if (!s_con || !s_wired)
        return;

    if (nm_setting_connection_get_id(s_con))
        m_nameEdit->setText(TQString::fromUtf8(nm_setting_connection_get_id(s_con)));

    m_autoconnectCheck->setChecked(
        nm_setting_connection_get_autoconnect(s_con) ? true : false);

    mac = nm_setting_wired_get_mac_address(s_wired);
    if (!mac || !mac[0])
        mac = nm_setting_wired_get_cloned_mac_address(s_wired);
    if (mac && mac[0])
        m_macEdit->setText(TQString::fromUtf8(mac));

    ipv4EditorLoad(conn, &ipState);
    m_manualIpCheck->setChecked(ipState.manual);
    m_ipEdit->setText(ipState.address);
    m_netmaskEdit->setText(ipState.netmask);
    m_gatewayEdit->setText(ipState.gateway);
    m_dnsEdit->setText(ipState.dns);
    m_dnsSearchEdit->setText(ipState.dnsSearch);
}

bool WiredConnectionEditorDialog::saveProfile(TQString *errorOut)
{
    NMClient *client = m_client ? m_client->nmClient() : 0;
    NMConnection *conn;
    NMSettingConnection *s_con;
    NMSettingWired *s_wired;
    Ipv4EditorState ipState;
    TQString name;
    TQString mac;

    if (!client) {
        if (errorOut)
            *errorOut = i18n("NetworkManager is not available.");
        return false;
    }

    conn = editableConnection();
    s_con = conn ? nm_connection_get_setting_connection(conn) : 0;
    s_wired = conn ? nm_connection_get_setting_wired(conn) : 0;
    if (!conn || !s_con || !s_wired) {
        if (errorOut)
            *errorOut = i18n("Invalid wired connection.");
        return false;
    }

    name = m_nameEdit->text().stripWhiteSpace();
    mac = m_macEdit->text().stripWhiteSpace();
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

    if (mac.isEmpty()) {
        g_object_set(G_OBJECT(s_wired),
                     NM_SETTING_WIRED_MAC_ADDRESS, NULL,
                     NM_SETTING_WIRED_CLONED_MAC_ADDRESS, NULL,
                     NULL);
    } else {
        g_object_set(G_OBJECT(s_wired),
                     NM_SETTING_WIRED_MAC_ADDRESS, mac.utf8().data(),
                     NM_SETTING_WIRED_CLONED_MAC_ADDRESS, NULL,
                     NULL);
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

    return commitConnectionProfile(NM_REMOTE_CONNECTION(conn), errorOut);
}

void WiredConnectionEditorDialog::showEvent(TQShowEvent *event)
{
    TQDialog::showEvent(event);
    centerEditorDialog(this);
}

void WiredConnectionEditorDialog::onAccept()
{
    TQString error;
    TQString title = m_isNew
        ? i18n("New Wired Connection")
        : i18n("Edit Wired Connection");
    if (!saveProfile(&error)) {
        KMessageBox::error(this, error, title);
        return;
    }
    accept();
}

#include "wiredconnectioneditordialog.moc"
