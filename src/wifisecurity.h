#ifndef WIFI_SECURITY_H
#define WIFI_SECURITY_H

#include <tqstring.h>

typedef struct _NMClient NMClient;
typedef struct _NMConnection NMConnection;
typedef struct _NMDeviceWifi NMDeviceWifi;

enum WifiSecurityType {
    WifiSecNone = 0,
    WifiSecWepKey,
    WifiSecWepPassphrase,
    WifiSecLeap,
    WifiSecDynamicWep,
    WifiSecWpaPsk,
    WifiSecWpaEnterprise,
    WifiSecSae,
    WifiSecOwe,
    WifiSec_Count
};

TQString wifiSecurityLabel(WifiSecurityType type);
bool wifiSecurityNeedsSecret(WifiSecurityType type);
bool wifiSecurityNeedsIdentity(WifiSecurityType type);
TQString wifiSecuritySecretLabel(WifiSecurityType type);
bool wifiSecurityAvailable(WifiSecurityType type, NMDeviceWifi *wifi);
bool wifiSecurityAvailableOnClient(WifiSecurityType type, NMClient *client);
WifiSecurityType wifiSecurityDetect(NMConnection *conn);
void wifiSecurityClear(NMConnection *conn);
TQString wifiSecurityReadIdentity(NMConnection *conn);
bool wifiSecurityApply(NMConnection *conn, WifiSecurityType type,
                       const TQString &identity, const TQString &secret,
                       TQString *errorOut);

#endif
