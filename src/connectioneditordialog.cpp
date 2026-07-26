#include "connectioneditordialog.h"
#include "connectioneditorutil.h"
#include "wificonnectioneditordialog.h"
#include "wiredconnectioneditordialog.h"
#include "vpnconnectioneditordialog.h"
#include "icons.h"
#include "nm/nmclient.h"
#include "nm/nmeventpump.h"
#include "nm/glib_compat.h"

#include <tdelocale.h>
#include <tdelistview.h>
#include <tqlabel.h>
#include <tqpushbutton.h>
#include <tqlayout.h>
#include <tdemessagebox.h>
#include <tqcombobox.h>
#include <tqapplication.h>
#include <tqdesktopwidget.h>
#include <tqevent.h>

class ConnectionListItem : public TDEListViewItem
{
public:
    ConnectionListItem(TQListView *parent, const TQString &name,
                       const TQString &typeLabel, const TQString &path,
                       const TQPixmap &icon)
        : TDEListViewItem(parent, name, typeLabel)
        , m_path(path)
    {
        setPixmap(0, icon);
    }

    TQString path() const { return m_path; }

private:
    TQString m_path;
};

class NewConnectionTypeDialog : public TQDialog
{
public:
    enum Choice {
        ChoiceWifi = 0,
        ChoiceWired,
        ChoiceVpn
    };

    NewConnectionTypeDialog(TQWidget *parent = 0)
        : TQDialog(parent, "new_connection_type", true)
        , m_combo(0)
    {
        setCaption(i18n("New Connection"));
        NmIcons::applyDialogIcon(this, NmIcons::appIconName());

        TQVBoxLayout *root = new TQVBoxLayout(this, 12, 6);
        TQHBoxLayout *row = new TQHBoxLayout();
        row->setSpacing(6);
        row->addWidget(new TQLabel(i18n("Connection type:"), this));
        m_combo = new TQComboBox(this);
        m_combo->insertItem(i18n("Wi-Fi"));
        m_combo->insertItem(i18n("Wired"));
        m_combo->insertItem(i18n("VPN"));
        row->addWidget(m_combo, 1);
        root->addLayout(row);

        TQHBoxLayout *buttons = new TQHBoxLayout();
        buttons->setSpacing(6);
        buttons->addStretch(1);
        TQPushButton *cancelBtn = new TQPushButton(i18n("Cancel"), this);
        TQPushButton *okBtn = new TQPushButton(i18n("Continue"), this);
        okBtn->setDefault(true);
        buttons->addWidget(cancelBtn);
        buttons->addWidget(okBtn);
        root->addLayout(buttons);

        connect(okBtn, TQT_SIGNAL(clicked()), this, TQT_SLOT(accept()));
        connect(cancelBtn, TQT_SIGNAL(clicked()), this, TQT_SLOT(reject()));
        resize(420, 120);
    }

    Choice selectedChoice() const
    {
        int index = m_combo ? m_combo->currentItem() : 0;
        if (index == ChoiceWired)
            return ChoiceWired;
        if (index == ChoiceVpn)
            return ChoiceVpn;
        return ChoiceWifi;
    }

protected:
    void showEvent(TQShowEvent *event)
    {
        TQDialog::showEvent(event);
        centerEditorDialog(this);
    }

private:
    TQComboBox *m_combo;
};

class VpnTypePickerDialog : public TQDialog
{
public:
    VpnTypePickerDialog(TQWidget *parent = 0)
        : TQDialog(parent, "vpn_type_picker", true)
        , m_combo(0)
    {
        setCaption(i18n("New VPN Connection"));

        TQVBoxLayout *root = new TQVBoxLayout(this, 12, 6);
        TQHBoxLayout *row = new TQHBoxLayout();
        row->setSpacing(6);
        row->addWidget(new TQLabel(i18n("VPN type:"), this));
        m_combo = new TQComboBox(this);
        row->addWidget(m_combo, 1);
        root->addLayout(row);

        struct VpnTypeEntry {
            const char *service;
            const char *label;
        };
        static const VpnTypeEntry entries[] = {
            { "org.freedesktop.NetworkManager.openvpn", "OpenVPN" },
            { "org.freedesktop.NetworkManager.vpnc", "Cisco VPN" },
            { "org.freedesktop.NetworkManager.openconnect", "OpenConnect" },
            { "org.freedesktop.NetworkManager.l2tp", "L2TP" },
            { "org.freedesktop.NetworkManager.pptp", "PPTP" },
            { "org.freedesktop.NetworkManager.wireguard", "WireGuard" },
        };

        for (uint i = 0; i < sizeof(entries) / sizeof(entries[0]); ++i) {
            m_services.append(TQString::fromUtf8(entries[i].service));
            m_combo->insertItem(i18n(entries[i].label));
        }

        TQHBoxLayout *buttons = new TQHBoxLayout();
        buttons->setSpacing(6);
        buttons->addStretch(1);
        TQPushButton *cancelBtn = new TQPushButton(i18n("Cancel"), this);
        TQPushButton *okBtn = new TQPushButton(i18n("Continue"), this);
        okBtn->setDefault(true);
        buttons->addWidget(cancelBtn);
        buttons->addWidget(okBtn);
        root->addLayout(buttons);

        connect(okBtn, TQT_SIGNAL(clicked()), this, TQT_SLOT(accept()));
        connect(cancelBtn, TQT_SIGNAL(clicked()), this, TQT_SLOT(reject()));
        resize(420, 120);
    }

    TQString selectedService() const
    {
        int index = m_combo ? m_combo->currentItem() : 0;
        if (index < 0 || index >= (int) m_services.size())
            return TQString::fromUtf8("org.freedesktop.NetworkManager.openvpn");
        return m_services[(uint) index];
    }

protected:
    void showEvent(TQShowEvent *event)
    {
        TQDialog::showEvent(event);
        centerEditorDialog(this);
    }

private:
    TQComboBox *m_combo;
    TQValueList<TQString> m_services;
};

static void waitForConnectionRemoved(NMClient *client, const TQString &path)
{
    if (!client || path.isEmpty())
        return;

    for (int i = 0; i < 50; ++i) {
        NmEventPump::pumpUi();
        if (!nm_client_get_connection_by_path(client, path.utf8().data()))
            return;
        g_usleep(10000);
    }
}

static void waitForConnectionAdded(NMClient *client, const TQString &path)
{
    if (!client || path.isEmpty())
        return;

    for (int i = 0; i < 50; ++i) {
        NmEventPump::pumpUi();
        if (nm_client_get_connection_by_path(client, path.utf8().data()))
            return;
        g_usleep(10000);
    }
}

ConnectionEditorDialog::ConnectionEditorDialog(NmClient *client, TQWidget *parent)
    : TQDialog(parent, "connection_editor", false)
    , m_client(client)
    , m_list(0)
    , m_newBtn(0)
{
    setCaption(i18n("Edit Connections"));
    NmIcons::applyDialogIcon(this, NmIcons::appIconName());

    TQVBoxLayout *root = new TQVBoxLayout(this, 12, 6);

    m_list = new TDEListView(this);
    m_list->addColumn(i18n("Name"));
    m_list->addColumn(i18n("Type"));
    m_list->setAllColumnsShowFocus(true);
    m_list->setFullWidth(true);
    root->addWidget(m_list, 1);

    TQHBoxLayout *buttons = new TQHBoxLayout();
    buttons->setSpacing(6);
    m_newBtn = new TQPushButton(i18n("New..."), this);
    TQPushButton *deleteBtn = new TQPushButton(i18n("Delete"), this);
    TQPushButton *editBtn = new TQPushButton(i18n("Edit"), this);
    TQPushButton *closeBtn = new TQPushButton(i18n("Close"), this);
    buttons->addWidget(m_newBtn);
    buttons->addWidget(deleteBtn);
    buttons->addWidget(editBtn);
    buttons->addStretch(1);
    buttons->addWidget(closeBtn);
    root->addLayout(buttons);

    connect(m_newBtn, TQT_SIGNAL(clicked()), this, TQT_SLOT(onNew()));
    connect(deleteBtn, TQT_SIGNAL(clicked()), this, TQT_SLOT(onDelete()));
    connect(editBtn, TQT_SIGNAL(clicked()), this, TQT_SLOT(onEdit()));
    connect(closeBtn, TQT_SIGNAL(clicked()), this, TQT_SLOT(close()));
    connect(m_list, TQT_SIGNAL(selectionChanged()),
            this, TQT_SLOT(onSelectionChanged()));

    reload();
    onSelectionChanged();
    resize(520, 460);
}

void ConnectionEditorDialog::showEvent(TQShowEvent *event)
{
    TQDialog::showEvent(event);

    TQDesktopWidget *desktop = TQApplication::desktop();
    int screen = desktop->screenNumber(this);
    TQRect area = desktop->availableGeometry(screen);
    move(area.x() + (area.width() - width()) / 2,
         area.y() + (area.height() - height()) / 2);
}

void ConnectionEditorDialog::reload()
{
    NMClient *client = m_client ? m_client->nmClient() : 0;
    m_list->clear();

    if (!client)
        return;

    const GPtrArray *connections = nm_client_get_connections(client);
    for (guint i = 0; connections && i < connections->len; ++i) {
        NMRemoteConnection *conn = (NMRemoteConnection *) g_ptr_array_index(connections, i);
        NMSettingConnection *s_con;
        const char *id;
        ConnectionProfileUi profileUi;

        if (!isConnectionEditorVisible(NM_CONNECTION(conn)))
            continue;

        s_con = nm_connection_get_setting_connection(NM_CONNECTION(conn));
        if (!s_con)
            continue;

        id = nm_setting_connection_get_id(s_con);
        if (!id)
            continue;

        profileUi = connectionProfileUi(NM_CONNECTION(conn));

        TDEListViewItem *item = new ConnectionListItem(
            m_list, TQString::fromUtf8(id), profileUi.typeLabel,
            TQString::fromUtf8(nm_connection_get_path(NM_CONNECTION(conn))),
            NmIcons::menuPixmap(profileUi.iconName));
        (void) item;
    }

    if (m_list->childCount() > 0)
        m_list->setSelected(m_list->firstChild(), true);

    m_list->triggerUpdate();
}

TQString ConnectionEditorDialog::selectedConnectionPath() const
{
    ConnectionListItem *item = (ConnectionListItem *) m_list->selectedItem();
    if (!item)
        return TQString::null;
    return item->path();
}

void ConnectionEditorDialog::onSelectionChanged()
{
}

void ConnectionEditorDialog::onNew()
{
    if (!m_client)
        return;

    NMClient *client = m_client->nmClient();

    NewConnectionTypeDialog picker(this);
    if (picker.exec() != TQDialog::Accepted)
        return;

    switch (picker.selectedChoice()) {
    case NewConnectionTypeDialog::ChoiceWifi: {
        NMConnection *conn = newWifiConnectionTemplate();
        WifiConnectionEditorDialog dlg(m_client, conn, this);
        if (dlg.exec() == TQDialog::Accepted) {
            waitForConnectionAdded(client, dlg.connectionPath());
            reload();
            onSelectionChanged();
        }
        break;
    }
    case NewConnectionTypeDialog::ChoiceWired: {
        NMConnection *conn = newWiredConnectionTemplate();
        WiredConnectionEditorDialog dlg(m_client, conn, this);
        if (dlg.exec() == TQDialog::Accepted) {
            waitForConnectionAdded(client, dlg.connectionPath());
            reload();
            onSelectionChanged();
        }
        break;
    }
    case NewConnectionTypeDialog::ChoiceVpn: {
        VpnTypePickerDialog vpnPicker(this);
        if (vpnPicker.exec() != TQDialog::Accepted)
            return;
        NMConnection *conn = newVpnConnectionTemplate(
            vpnPicker.selectedService().utf8().data());
        VpnConnectionEditorDialog dlg(m_client, conn, this);
        if (dlg.exec() == TQDialog::Accepted) {
            waitForConnectionAdded(client, dlg.connectionPath());
            reload();
            onSelectionChanged();
        }
        break;
    }
    default:
        break;
    }
}

void ConnectionEditorDialog::onDelete()
{
    TQString path = selectedConnectionPath();
    NMClient *client = m_client ? m_client->nmClient() : 0;
    NMRemoteConnection *conn;
    GError *error = 0;

    if (path.isEmpty() || !client)
        return;

    conn = nm_client_get_connection_by_path(client, path.utf8().data());
    if (!conn)
        return;

    NMSettingConnection *s_con = nm_connection_get_setting_connection(NM_CONNECTION(conn));
    const char *id = s_con ? nm_setting_connection_get_id(s_con) : 0;
    TQString name = id ? TQString::fromUtf8(id) : path;

    int answer = NmIcons::questionYesNo(
        this,
        i18n("Delete connection \"%1\"?").arg(name),
        i18n("Delete Connection"));

    if (answer != KMessageBox::Yes)
        return;

    if (!nm_remote_connection_delete(conn, 0, &error)) {
        TQString msg = error && error->message
            ? TQString::fromUtf8(error->message)
            : i18n("Failed to delete connection.");
        KMessageBox::error(this, msg, i18n("Delete Connection"));
        if (error)
            g_error_free(error);
        return;
    }

    waitForConnectionRemoved(client, path);
    reload();
    onSelectionChanged();
}

void ConnectionEditorDialog::onEdit()
{
    TQString path = selectedConnectionPath();
    NMClient *client = m_client ? m_client->nmClient() : 0;
    NMRemoteConnection *conn;

    if (path.isEmpty() || !client)
        return;

    conn = nm_client_get_connection_by_path(client, path.utf8().data());
    if (!conn)
        return;

    if (isWirelessConnection(NM_CONNECTION(conn))) {
        WifiConnectionEditorDialog dlg(m_client, path, this);
        if (dlg.exec() == TQDialog::Accepted)
            reload();
        return;
    }

    if (isWiredConnection(NM_CONNECTION(conn))) {
        WiredConnectionEditorDialog dlg(m_client, path, this);
        if (dlg.exec() == TQDialog::Accepted)
            reload();
        return;
    }

    if (isVpnConnection(NM_CONNECTION(conn))) {
        VpnConnectionEditorDialog dlg(m_client, path, this);
        if (dlg.exec() == TQDialog::Accepted)
            reload();
        return;
    }

    KMessageBox::information(
        this,
        i18n("Editing for this connection type is not implemented yet."),
        i18n("Edit Connection"));
}

#include "connectioneditordialog.moc"
