#ifndef WIRED_CONNECTION_EDITOR_DIALOG_H
#define WIRED_CONNECTION_EDITOR_DIALOG_H

#include <tqdialog.h>
#include <tqstring.h>

class TQShowEvent;
class TQLineEdit;
class TQCheckBox;
class TQGroupBox;
class NmClient;

typedef struct _NMConnection NMConnection;

class WiredConnectionEditorDialog : public TQDialog
{
    TQ_OBJECT

public:
    explicit WiredConnectionEditorDialog(NmClient *client,
                                         const TQString &connectionPath,
                                         TQWidget *parent = 0);
    explicit WiredConnectionEditorDialog(NmClient *client,
                                         NMConnection *newConnection,
                                         TQWidget *parent = 0);
    ~WiredConnectionEditorDialog();

    TQString connectionPath() const { return m_connectionPath; }

protected:
    void showEvent(TQShowEvent *event);

private slots:
    void onManualIpToggled(bool enabled);
    void onAccept();

private:
    void loadFromConnectionObject(NMConnection *conn);
    void loadFromConnection();
    bool saveProfile(TQString *errorOut);
    NMConnection *editableConnection();
    void buildUi(const TQString &caption);

    NmClient *m_client;
    TQString m_connectionPath;
    bool m_isNew;
    NMConnection *m_newConnection;

    TQLineEdit *m_nameEdit;
    TQLineEdit *m_macEdit;
    TQCheckBox *m_autoconnectCheck;
    TQCheckBox *m_manualIpCheck;
    TQGroupBox *m_manualIpGroup;
    TQLineEdit *m_ipEdit;
    TQLineEdit *m_netmaskEdit;
    TQLineEdit *m_gatewayEdit;
    TQLineEdit *m_dnsEdit;
    TQLineEdit *m_dnsSearchEdit;
};

#endif
