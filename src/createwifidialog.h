#ifndef CREATE_WIFI_DIALOG_H
#define CREATE_WIFI_DIALOG_H

#include <tqdialog.h>
#include <tqstring.h>
#include <tqvaluelist.h>

#include "wifisecurity.h"

class TQShowEvent;
class TQLabel;
class TQComboBox;
class TQLineEdit;
class NmData;

class CreateWifiDialog : public TQDialog
{
    TQ_OBJECT

public:
    explicit CreateWifiDialog(NmData *data, TQWidget *parent = 0);

protected:
    void showEvent(TQShowEvent *event);

private slots:
    void onSecurityChanged(int index);
    void onAccept();

private:
    void populateSecurityCombo();
    void updateCredentialFields();
    WifiSecurityType currentSecurityType() const;

    NmData *m_data;
    TQLineEdit *m_ssidEdit;
    TQComboBox *m_securityCombo;
    TQLabel *m_secretLabel;
    TQLineEdit *m_secretEdit;
    TQValueList<WifiSecurityType> m_securityTypes;
};

#endif
