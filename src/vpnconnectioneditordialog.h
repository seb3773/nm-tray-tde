#ifndef VPN_CONNECTION_EDITOR_DIALOG_H
#define VPN_CONNECTION_EDITOR_DIALOG_H

#include <tqdialog.h>
#include <tqstring.h>

class TQShowEvent;
class TQLabel;
class TQLineEdit;
class TQCheckBox;
class TQGroupBox;
class TQVBoxLayout;
class NmClient;

typedef struct _NMConnection NMConnection;

class VpnConnectionEditorDialog : public TQDialog
{
    TQ_OBJECT

public:
    explicit VpnConnectionEditorDialog(NmClient *client,
                                       const TQString &connectionPath,
                                       TQWidget *parent = 0);
    explicit VpnConnectionEditorDialog(NmClient *client,
                                       NMConnection *newConnection,
                                       TQWidget *parent = 0);
    ~VpnConnectionEditorDialog();

    TQString connectionPath() const { return m_connectionPath; }

protected:
    void showEvent(TQShowEvent *event);

private slots:
    void onIpsecToggled(bool enabled);
    void onAccept();

private:
    struct EditorRow {
        TQLabel *label;
        TQLineEdit *edit;

        EditorRow()
            : label(0)
            , edit(0)
        {
        }
    };

    enum VpnEditorKind {
        VpnKindGeneric = 0,
        VpnKindOpenVpn,
        VpnKindVpnc,
        VpnKindOpenConnect,
        VpnKindL2tp,
        VpnKindPptp,
        VpnKindWireGuard,
    };

    void buildUi(const TQString &caption);
    void addEditorRow(TQWidget *parent, TQVBoxLayout *root, const TQString &label,
                      EditorRow *row, bool password = false);
    void setRowShown(const EditorRow &row, bool shown);
    void applyFieldVisibility();
    void loadFromConnectionObject(NMConnection *conn);
    void loadFromConnection();
    bool saveProfile(TQString *errorOut);
    NMConnection *editableConnection();
    VpnEditorKind detectKind(const char *service, NMConnection *conn) const;

    NmClient *m_client;
    TQString m_connectionPath;
    bool m_isNew;
    NMConnection *m_newConnection;
    VpnEditorKind m_kind;
    TQString m_serverKey;
    TQString m_passwordKey;

    TQLineEdit *m_nameEdit;
    TQLabel *m_typeLabel;
    TQCheckBox *m_autoconnectCheck;

    EditorRow m_serverRow;
    EditorRow m_userRow;
    EditorRow m_passwordRow;
    TQLabel *m_passwordHint;

    EditorRow m_groupRow;
    EditorRow m_caRow;
    EditorRow m_certRow;
    EditorRow m_keyRow;

    TQCheckBox *m_ipsecCheck;
    TQGroupBox *m_ipsecGroup;
    TQLineEdit *m_ipsecPskEdit;

    EditorRow m_privateKeyRow;
    EditorRow m_publicKeyRow;
    EditorRow m_endpointRow;
    EditorRow m_allowedIpsRow;
    EditorRow m_keepaliveRow;
    TQLabel *m_wireguardHint;
};

#endif
