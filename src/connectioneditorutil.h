#ifndef CONNECTION_EDITOR_UTIL_H
#define CONNECTION_EDITOR_UTIL_H

#include <tqstring.h>

class TQDialog;
class TQWidget;
class TQVBoxLayout;

typedef struct _NMConnection NMConnection;
typedef struct _NMRemoteConnection NMRemoteConnection;
typedef struct _NMClient NMClient;

void centerEditorDialog(TQDialog *dialog);
bool commitConnectionProfile(NMRemoteConnection *remote, TQString *errorOut);
bool addConnectionProfile(NMClient *client, NMConnection *connection,
                          TQString *pathOut, TQString *errorOut);

NMConnection *newWifiConnectionTemplate();
NMConnection *newWiredConnectionTemplate();
NMConnection *newVpnConnectionTemplate(const char *serviceType);

bool isWirelessConnection(NMConnection *conn);
bool isWiredConnection(NMConnection *conn);
bool isVpnConnection(NMConnection *conn);
bool isConnectionEditorVisible(NMConnection *conn);

struct ConnectionProfileUi {
    TQString typeLabel;
    TQString iconName;
};
ConnectionProfileUi connectionProfileUi(NMConnection *conn);

typedef struct _NMSettingWireGuard NMSettingWireGuard;
NMSettingWireGuard *connectionWireguardSetting(NMConnection *conn);

#endif
