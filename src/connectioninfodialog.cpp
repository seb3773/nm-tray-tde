#include "connectioninfodialog.h"
#include "connectioneditorutil.h"
#include "connectioninfobuilder.h"
#include "icons.h"
#include "nm/nmclient.h"
#include "nm/nmeventpump.h"

#include <tdelocale.h>
#include <tqpushbutton.h>
#include <tqlayout.h>
#include <tqmultilineedit.h>
#include <tqevent.h>

ConnectionInfoDialog::ConnectionInfoDialog(NmClient *client, TQWidget *parent)
    : TQDialog(parent, "connection_info", false)
    , m_client(client)
    , m_text(0)
{
    setCaption(i18n("Connection Information"));

    TQVBoxLayout *root = new TQVBoxLayout(this, 12, 6);

    m_text = new TQMultiLineEdit(this);
    m_text->setReadOnly(true);
    m_text->setWordWrap(TQMultiLineEdit::WidgetWidth);
    root->addWidget(m_text, 1);

    TQHBoxLayout *buttons = new TQHBoxLayout();
    buttons->setSpacing(6);
    TQPushButton *refreshBtn = new TQPushButton(i18n("Refresh"), this);
    TQPushButton *closeBtn = new TQPushButton(i18n("Close"), this);
    closeBtn->setDefault(true);
    buttons->addWidget(refreshBtn);
    buttons->addStretch(1);
    buttons->addWidget(closeBtn);
    root->addLayout(buttons);

    connect(refreshBtn, TQT_SIGNAL(clicked()), this, TQT_SLOT(onRefresh()));
    connect(closeBtn, TQT_SIGNAL(clicked()), this, TQT_SLOT(close()));

    resize(560, 480);
}

void ConnectionInfoDialog::showEvent(TQShowEvent *event)
{
    TQDialog::showEvent(event);
    centerEditorDialog(this);
    reload();
}

void ConnectionInfoDialog::onRefresh()
{
    NmEventPump::pump();
    reload();
}

void ConnectionInfoDialog::reload()
{
    NMClient *client = m_client ? m_client->nmClient() : 0;
    TQValueList<ConnectionInfoEntry> entries = buildActiveConnectionInfoEntries(client);
    TQString body;
    uint i;

    if (entries.isEmpty()) {
        m_text->setText(i18n("No active connection."));
        NmIcons::applyDialogIcon(this, NmIcons::notConnectedIcon());
        return;
    }

    NmIcons::applyDialogIcon(this, entries[0].iconName);

    for (i = 0; i < entries.size(); ++i) {
        if (!body.isEmpty())
            body += "\n\n";
        if (entries.size() > 1)
            body += TQString("=== %1 ===\n").arg(entries[i].tabTitle);
        body += entries[i].body;
    }

    m_text->setText(body);
}

#include "connectioninfodialog.moc"
