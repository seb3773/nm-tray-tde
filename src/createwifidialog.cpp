#include "createwifidialog.h"
#include "nmdata.h"
#include "icons.h"

#include <tdelocale.h>
#include <tqlabel.h>
#include <tqlineedit.h>
#include <tqcombobox.h>
#include <tqpushbutton.h>
#include <tqlayout.h>
#include <tdemessagebox.h>
#include <tqapplication.h>
#include <tqdesktopwidget.h>
#include <tqevent.h>

CreateWifiDialog::CreateWifiDialog(NmData *data, TQWidget *parent)
    : TQDialog(parent, "create_wifi_dialog", true)
    , m_data(data)
    , m_ssidEdit(0)
    , m_securityCombo(0)
    , m_secretLabel(0)
    , m_secretEdit(0)
{
    setCaption(i18n("Create New Wi-Fi Network"));
    NmIcons::applyDialogIcon(this, NmIcons::wirelessIcon());

    TQVBoxLayout *root = new TQVBoxLayout(this, 12, 6);

    TQLabel *hint = new TQLabel(
        i18n("Create a Wi-Fi hotspot that shares your active Internet "
             "connection (for example Ethernet).\n"
             "Note: this may disconnect the current Wi-Fi client link "
             "on this adapter."),
        this);
    hint->setAlignment(TQt::AlignLeft | TQt::AlignTop);
    root->addWidget(hint);

    const int labelWidth = 150;

    TQHBoxLayout *ssidRow = new TQHBoxLayout();
    ssidRow->setSpacing(6);
    TQLabel *ssidLabel = new TQLabel(i18n("Network name (SSID):"), this);
    ssidLabel->setFixedWidth(labelWidth);
    ssidRow->addWidget(ssidLabel);
    m_ssidEdit = new TQLineEdit(this);
    ssidRow->addWidget(m_ssidEdit, 1);
    root->addLayout(ssidRow);

    TQHBoxLayout *secRow = new TQHBoxLayout();
    secRow->setSpacing(6);
    TQLabel *secLabel = new TQLabel(i18n("Security:"), this);
    secLabel->setFixedWidth(labelWidth);
    secRow->addWidget(secLabel);
    m_securityCombo = new TQComboBox(this);
    secRow->addWidget(m_securityCombo, 1);
    root->addLayout(secRow);

    TQHBoxLayout *secretRow = new TQHBoxLayout();
    secretRow->setSpacing(6);
    m_secretLabel = new TQLabel(i18n("Password:"), this);
    m_secretLabel->setFixedWidth(labelWidth);
    m_secretEdit = new TQLineEdit(this);
    m_secretEdit->setEchoMode(TQLineEdit::Password);
    secretRow->addWidget(m_secretLabel);
    secretRow->addWidget(m_secretEdit, 1);
    root->addLayout(secretRow);

    TQHBoxLayout *buttons = new TQHBoxLayout();
    buttons->setSpacing(6);
    buttons->addStretch(1);
    TQPushButton *cancelBtn = new TQPushButton(i18n("Cancel"), this);
    TQPushButton *okBtn = new TQPushButton(i18n("Create"), this);
    okBtn->setDefault(true);
    buttons->addWidget(cancelBtn);
    buttons->addWidget(okBtn);
    root->addLayout(buttons);

    connect(m_securityCombo, TQT_SIGNAL(activated(int)),
            this, TQT_SLOT(onSecurityChanged(int)));
    connect(okBtn, TQT_SIGNAL(clicked()), this, TQT_SLOT(onAccept()));
    connect(cancelBtn, TQT_SIGNAL(clicked()), this, TQT_SLOT(reject()));

    populateSecurityCombo();
    if (m_securityCombo->count() > 0) {
        int defaultIndex = 0;
        for (uint i = 0; i < m_securityTypes.size(); ++i) {
            if (m_securityTypes[i] == WifiSecWpaPsk) {
                defaultIndex = (int) i;
                break;
            }
        }
        m_securityCombo->setCurrentItem(defaultIndex);
        onSecurityChanged(defaultIndex);
    }

    setFixedWidth(560);
    adjustSize();
}

void CreateWifiDialog::populateSecurityCombo()
{
    static const WifiSecurityType kHotspotTypes[] = {
        WifiSecNone,
        WifiSecWpaPsk,
        WifiSecSae
    };

    m_securityCombo->clear();
    m_securityTypes.clear();

    for (uint i = 0; i < sizeof(kHotspotTypes) / sizeof(kHotspotTypes[0]); ++i) {
        WifiSecurityType type = kHotspotTypes[i];
        if (m_data && !m_data->isCreateWifiSecurityAvailable(type))
            continue;
        m_securityTypes.append(type);
        m_securityCombo->insertItem(wifiSecurityLabel(type));
    }

    if (m_securityCombo->count() == 0) {
        m_securityTypes.append(WifiSecWpaPsk);
        m_securityCombo->insertItem(wifiSecurityLabel(WifiSecWpaPsk));
    }
}

WifiSecurityType CreateWifiDialog::currentSecurityType() const
{
    int index = m_securityCombo->currentItem();
    if (index < 0 || index >= (int) m_securityTypes.size())
        return WifiSecWpaPsk;
    return m_securityTypes[(uint) index];
}

void CreateWifiDialog::updateCredentialFields()
{
    WifiSecurityType type = currentSecurityType();
    bool needSecret = wifiSecurityNeedsSecret(type);

    if (needSecret) {
        m_secretLabel->show();
        m_secretEdit->show();
        m_secretLabel->setText(wifiSecuritySecretLabel(type));
    } else {
        m_secretLabel->hide();
        m_secretEdit->hide();
        m_secretEdit->clear();
    }
}

void CreateWifiDialog::showEvent(TQShowEvent *event)
{
    TQDialog::showEvent(event);

    TQDesktopWidget *desktop = TQApplication::desktop();
    int screen = desktop->screenNumber(this);
    TQRect area = desktop->availableGeometry(screen);
    move(area.x() + (area.width() - width()) / 2,
         area.y() + (area.height() - height()) / 2);
}

void CreateWifiDialog::onSecurityChanged(int index)
{
    (void) index;
    updateCredentialFields();
}

void CreateWifiDialog::onAccept()
{
    TQString ssid = m_ssidEdit->text().stripWhiteSpace();
    WifiSecurityType type = currentSecurityType();
    TQString password = m_secretEdit->text();

    if (ssid.isEmpty()) {
        KMessageBox::error(this, i18n("Please enter the network name (SSID)."),
                           i18n("Create New Wi-Fi Network"));
        return;
    }

    if (wifiSecurityNeedsSecret(type) && password.isEmpty()) {
        KMessageBox::error(this,
                           i18n("Please enter a password (8-63 characters)."),
                           i18n("Create New Wi-Fi Network"));
        return;
    }

    TQString error;
    if (!m_data->createWifiHotspot(ssid, type, password, &error)) {
        KMessageBox::error(this, error, i18n("Create New Wi-Fi Network"));
        return;
    }

    accept();
}

#include "createwifidialog.moc"
