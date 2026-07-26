#ifndef WIFI_CONNECTION_EDITOR_DIALOG_H
#define WIFI_CONNECTION_EDITOR_DIALOG_H

#include <tqdialog.h>
#include <tqstring.h>
#include <tqvaluelist.h>

#include "wifisecurity.h"

class TQShowEvent;
class TQLabel;
class TQComboBox;
class TQLineEdit;
class TQCheckBox;
class TQGroupBox;
class NmClient;

typedef struct _NMConnection NMConnection;

class WifiConnectionEditorDialog : public TQDialog
{
    TQ_OBJECT

public:
    explicit WifiConnectionEditorDialog(NmClient *client,
                                        const TQString &connectionPath,
                                        TQWidget *parent = 0);
    explicit WifiConnectionEditorDialog(NmClient *client,
                                        NMConnection *newConnection,
                                        TQWidget *parent = 0);
    ~WifiConnectionEditorDialog();

    TQString connectionPath() const { return m_connectionPath; }

protected:
    void showEvent(TQShowEvent *event);

private slots:
    void onSecurityChanged(int index);
    void onManualIpToggled(bool enabled);
    void onAccept();

private:
    void buildUi(const TQString &caption);
    void populateSecurityCombo();
    void updateCredentialFields();
    void loadFromConnectionObject(NMConnection *conn);
    void loadFromConnection();
    bool saveProfile(TQString *errorOut);
    NMConnection *editableConnection();
    WifiSecurityType currentSecurityType() const;

    NmClient *m_client;
    TQString m_connectionPath;
    bool m_isNew;
    NMConnection *m_newConnection;
    WifiSecurityType m_loadedSecurityType;

    TQLineEdit *m_nameEdit;
    TQLineEdit *m_ssidEdit;
    TQComboBox *m_securityCombo;
    TQLabel *m_usernameLabel;
    TQLineEdit *m_usernameEdit;
    TQLabel *m_secretLabel;
    TQLineEdit *m_secretEdit;
    TQCheckBox *m_autoconnectCheck;
    TQCheckBox *m_hiddenCheck;
    TQLineEdit *m_mtuEdit;
    TQCheckBox *m_manualIpCheck;
    TQGroupBox *m_manualIpGroup;
    TQLineEdit *m_ipEdit;
    TQLineEdit *m_netmaskEdit;
    TQLineEdit *m_gatewayEdit;
    TQLineEdit *m_dnsEdit;
    TQLineEdit *m_dnsSearchEdit;
    TQValueList<WifiSecurityType> m_securityTypes;
};

#endif
