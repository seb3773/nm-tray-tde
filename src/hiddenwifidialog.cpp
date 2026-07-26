#include "hiddenwifidialog.h"
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

HiddenWifiDialog::HiddenWifiDialog(NmData *data, TQWidget *parent)
    : TQDialog(parent, "hidden_wifi_dialog", true)
    , m_data(data)
    , m_ssidEdit(0)
    , m_securityCombo(0)
    , m_usernameLabel(0)
    , m_usernameEdit(0)
    , m_secretLabel(0)
    , m_secretEdit(0)
{
    setCaption(i18n("Connect to Hidden Wi-Fi Network"));
    NmIcons::applyDialogIcon(this, NmIcons::wirelessIcon());

    TQVBoxLayout *root = new TQVBoxLayout(this, 12, 6);

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

    TQHBoxLayout *buttons = new TQHBoxLayout();
    buttons->setSpacing(6);
    buttons->addStretch(1);
    TQPushButton *cancelBtn = new TQPushButton(i18n("Cancel"), this);
    TQPushButton *okBtn = new TQPushButton(i18n("Connect"), this);
    okBtn->setDefault(true);
    buttons->addWidget(cancelBtn);
    buttons->addWidget(okBtn);
    root->addLayout(buttons);

    connect(m_securityCombo, TQT_SIGNAL(activated(int)),
            this, TQT_SLOT(onSecurityChanged(int)));
    connect(okBtn, TQT_SIGNAL(clicked()), this, TQT_SLOT(onAccept()));
    connect(cancelBtn, TQT_SIGNAL(clicked()), this, TQT_SLOT(reject()));

    populateSecurityCombo();
    if (m_securityCombo->count() > 0)
        onSecurityChanged(0);
    resize(460, 210);
}

void HiddenWifiDialog::populateSecurityCombo()
{
    m_securityCombo->clear();
    m_securityTypes.clear();

    for (int i = WifiSecNone; i < WifiSec_Count; ++i) {
        WifiSecurityType type = (WifiSecurityType) i;
        if (m_data && !m_data->isHiddenWifiSecurityAvailable(type))
            continue;
        m_securityTypes.append(type);
        m_securityCombo->insertItem(wifiSecurityLabel(type));
    }

    if (m_securityCombo->count() == 0) {
        m_securityTypes.append(WifiSecNone);
        m_securityCombo->insertItem(wifiSecurityLabel(WifiSecNone));
    }
}

WifiSecurityType HiddenWifiDialog::currentSecurityType() const
{
    int index = m_securityCombo->currentItem();
    if (index < 0 || index >= (int) m_securityTypes.size())
        return WifiSecNone;
    return m_securityTypes[(uint) index];
}

void HiddenWifiDialog::updateCredentialFields()
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
        m_secretLabel->setText(wifiSecuritySecretLabel(type));
    } else {
        m_secretLabel->hide();
        m_secretEdit->hide();
        m_secretEdit->clear();
    }
}

void HiddenWifiDialog::showEvent(TQShowEvent *event)
{
    TQDialog::showEvent(event);

    TQDesktopWidget *desktop = TQApplication::desktop();
    int screen = desktop->screenNumber(this);
    TQRect area = desktop->availableGeometry(screen);
    move(area.x() + (area.width() - width()) / 2,
         area.y() + (area.height() - height()) / 2);
}

void HiddenWifiDialog::onSecurityChanged(int index)
{
    (void) index;
    updateCredentialFields();
}

void HiddenWifiDialog::onAccept()
{
    TQString ssid = m_ssidEdit->text().stripWhiteSpace();
    WifiSecurityType type = currentSecurityType();
    TQString identity = m_usernameEdit->text().stripWhiteSpace();
    TQString secret = m_secretEdit->text();

    if (ssid.isEmpty()) {
        KMessageBox::error(this, i18n("Please enter the network name (SSID)."),
                           i18n("Connect to Hidden Wi-Fi Network"));
        return;
    }

    if (wifiSecurityNeedsIdentity(type) && identity.isEmpty()) {
        KMessageBox::error(this, i18n("Please enter a username."),
                           i18n("Connect to Hidden Wi-Fi Network"));
        return;
    }

    if (wifiSecurityNeedsSecret(type) && secret.isEmpty()) {
        KMessageBox::error(this,
                           i18n("Please enter the required credentials."),
                           i18n("Connect to Hidden Wi-Fi Network"));
        return;
    }

    TQString error;
    if (!m_data->connectHiddenWifi(ssid, type, identity, secret, &error)) {
        KMessageBox::error(this, error, i18n("Connect to Hidden Wi-Fi Network"));
        return;
    }

    accept();
}

#include "hiddenwifidialog.moc"
