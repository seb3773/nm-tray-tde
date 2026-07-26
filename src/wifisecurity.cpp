#include "wifisecurity.h"
#include "wifiutil.h"

#include "nm/glib_compat.h"

#include <tdelocale.h>
#include <string.h>

static NMUtilsSecurityType nmSecurityType(WifiSecurityType type)
{
    switch (type) {
    case WifiSecNone:
        return NMU_SEC_NONE;
    case WifiSecWepKey:
    case WifiSecWepPassphrase:
        return NMU_SEC_STATIC_WEP;
    case WifiSecLeap:
        return NMU_SEC_LEAP;
    case WifiSecDynamicWep:
        return NMU_SEC_DYNAMIC_WEP;
    case WifiSecWpaPsk:
        return NMU_SEC_WPA2_PSK;
    case WifiSecWpaEnterprise:
        return NMU_SEC_WPA2_ENTERPRISE;
    case WifiSecSae:
        return NMU_SEC_SAE;
    case WifiSecOwe:
        return NMU_SEC_OWE;
    default:
        return NMU_SEC_INVALID;
    }
}

static NMDeviceWifiCapabilities deviceWifiCaps(NMDeviceWifi *wifi)
{
    if (wifi)
        return nm_device_wifi_get_capabilities(wifi);

    return (NMDeviceWifiCapabilities) (
        NM_WIFI_DEVICE_CAP_CIPHER_WEP40
        | NM_WIFI_DEVICE_CAP_CIPHER_WEP104
        | NM_WIFI_DEVICE_CAP_CIPHER_TKIP
        | NM_WIFI_DEVICE_CAP_CIPHER_CCMP
        | NM_WIFI_DEVICE_CAP_WPA
        | NM_WIFI_DEVICE_CAP_RSN);
}

static void set8021x(NMConnection *conn, const char *const *eap,
                     const TQString &identity, const TQString &secret,
                     const char *phase2Auth)
{
    NMSetting8021x *s_8021x = NM_SETTING_802_1X(nm_setting_802_1x_new());

    for (const char *const *p = eap; p && *p; ++p)
        nm_setting_802_1x_add_eap_method(s_8021x, *p);

    if (!identity.isEmpty()) {
        g_object_set(G_OBJECT(s_8021x),
                     NM_SETTING_802_1X_IDENTITY, identity.utf8().data(),
                     NULL);
    }
    if (!secret.isEmpty()) {
        g_object_set(G_OBJECT(s_8021x),
                     NM_SETTING_802_1X_PASSWORD, secret.utf8().data(),
                     NULL);
    }
    if (phase2Auth) {
        g_object_set(G_OBJECT(s_8021x),
                     NM_SETTING_802_1X_PHASE2_AUTH, phase2Auth,
                     NULL);
    }

    nm_connection_add_setting(conn, NM_SETTING(s_8021x));
}

TQString wifiSecurityLabel(WifiSecurityType type)
{
    switch (type) {
    case WifiSecNone:
        return i18n("None");
    case WifiSecWepKey:
        return i18n("WEP 40/128-bit Key (Hex or ASCII)");
    case WifiSecWepPassphrase:
        return i18n("WEP 128-bit Passphrase");
    case WifiSecLeap:
        return i18n("LEAP");
    case WifiSecDynamicWep:
        return i18n("Dynamic WEP (802.1X)");
    case WifiSecWpaPsk:
        return i18n("WPA/WPA2/WPA3 Personal");
    case WifiSecWpaEnterprise:
        return i18n("WPA/WPA2 Enterprise");
    case WifiSecSae:
        return i18n("WPA3 Personal");
    case WifiSecOwe:
        return i18n("Enhanced Open");
    default:
        return TQString::null;
    }
}

bool wifiSecurityNeedsSecret(WifiSecurityType type)
{
    switch (type) {
    case WifiSecNone:
    case WifiSecOwe:
        return false;
    default:
        return true;
    }
}

bool wifiSecurityNeedsIdentity(WifiSecurityType type)
{
    switch (type) {
    case WifiSecLeap:
    case WifiSecDynamicWep:
    case WifiSecWpaEnterprise:
        return true;
    default:
        return false;
    }
}

TQString wifiSecuritySecretLabel(WifiSecurityType type)
{
    switch (type) {
    case WifiSecWepKey:
        return i18n("WEP key:");
    case WifiSecWepPassphrase:
        return i18n("WEP passphrase:");
    case WifiSecLeap:
    case WifiSecDynamicWep:
    case WifiSecWpaEnterprise:
        return i18n("Password:");
    case WifiSecWpaPsk:
    case WifiSecSae:
        return i18n("Password:");
    default:
        return i18n("Password:");
    }
}

bool wifiSecurityAvailable(WifiSecurityType type, NMDeviceWifi *wifi)
{
    NMUtilsSecurityType nmType = nmSecurityType(type);
    NMDeviceWifiCapabilities caps = deviceWifiCaps(wifi);

    if (nmType == NMU_SEC_INVALID)
        return false;

    return nm_utils_security_valid(nmType, caps, FALSE, FALSE,
                                   NM_802_11_AP_FLAGS_NONE,
                                   NM_802_11_AP_SEC_NONE,
                                   NM_802_11_AP_SEC_NONE);
}

bool wifiSecurityAvailableOnClient(WifiSecurityType type, NMClient *client)
{
    if (!client)
        return wifiSecurityAvailable(type, 0);
    return wifiSecurityAvailable(type, pickPrimaryWifiDevice(client));
}

void wifiSecurityClear(NMConnection *conn)
{
    if (!conn)
        return;

    nm_connection_remove_setting(conn, NM_TYPE_SETTING_WIRELESS_SECURITY);
    nm_connection_remove_setting(conn, NM_TYPE_SETTING_802_1X);
}

WifiSecurityType wifiSecurityDetect(NMConnection *conn)
{
    NMSettingWirelessSecurity *s_wsec;
    const char *keyMgmt;
    const char *authAlg;

    if (!conn)
        return WifiSecNone;

    s_wsec = nm_connection_get_setting_wireless_security(conn);
    if (!s_wsec)
        return WifiSecNone;

    keyMgmt = nm_setting_wireless_security_get_key_mgmt(s_wsec);
    if (!keyMgmt || strcmp(keyMgmt, "none") == 0) {
        if (nm_setting_wireless_security_get_wep_key(s_wsec, 0)) {
            if (nm_setting_wireless_security_get_wep_key_type(s_wsec)
                == NM_WEP_KEY_TYPE_PASSPHRASE)
                return WifiSecWepPassphrase;
            return WifiSecWepKey;
        }
        return WifiSecNone;
    }

    if (strcmp(keyMgmt, "wpa-psk") == 0)
        return WifiSecWpaPsk;
    if (strcmp(keyMgmt, "sae") == 0)
        return WifiSecSae;
    if (strcmp(keyMgmt, "owe") == 0)
        return WifiSecOwe;
    if (strcmp(keyMgmt, "ieee8021x") == 0) {
        authAlg = nm_setting_wireless_security_get_auth_alg(s_wsec);
        if (authAlg && strcmp(authAlg, "leap") == 0)
            return WifiSecLeap;
        return WifiSecDynamicWep;
    }
    if (strcmp(keyMgmt, "wpa-eap") == 0)
        return WifiSecWpaEnterprise;

    return WifiSecNone;
}

TQString wifiSecurityReadIdentity(NMConnection *conn)
{
    NMSetting8021x *s_8021x;
    const char *identity;

    if (!conn)
        return TQString();

    s_8021x = nm_connection_get_setting_802_1x(conn);
    if (!s_8021x)
        return TQString();

    identity = nm_setting_802_1x_get_identity(s_8021x);
    if (!identity)
        return TQString();

    return TQString::fromUtf8(identity);
}

bool wifiSecurityApply(NMConnection *conn, WifiSecurityType type,
                       const TQString &identity, const TQString &secret,
                       TQString *errorOut)
{
    NMSettingWirelessSecurity *s_wsec = 0;
    NMWepKeyType wepType = NM_WEP_KEY_TYPE_UNKNOWN;
    static const char *eapLeap[] = { "leap", NULL };
    static const char *eapPeap[] = { "peap", NULL };

    if (!conn)
        return false;

    switch (type) {
    case WifiSecNone:
        return true;

    case WifiSecWepKey:
        wepType = NM_WEP_KEY_TYPE_KEY;
        if (!nm_utils_wep_key_valid(secret.utf8().data(), wepType)) {
            if (errorOut)
                *errorOut = i18n("Invalid WEP key.");
            return false;
        }
        s_wsec = NM_SETTING_WIRELESS_SECURITY(nm_setting_wireless_security_new());
        g_object_set(G_OBJECT(s_wsec),
                     NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "none",
                     NM_SETTING_WIRELESS_SECURITY_AUTH_ALG, "open",
                     NM_SETTING_WIRELESS_SECURITY_WEP_KEY_TYPE, wepType,
                     NM_SETTING_WIRELESS_SECURITY_WEP_TX_KEYIDX, 0,
                     NM_SETTING_WIRELESS_SECURITY_WEP_KEY0, secret.utf8().data(),
                     NULL);
        nm_connection_add_setting(conn, NM_SETTING(s_wsec));
        return true;

    case WifiSecWepPassphrase:
        wepType = NM_WEP_KEY_TYPE_PASSPHRASE;
        if (!nm_utils_wep_key_valid(secret.utf8().data(), wepType)) {
            if (errorOut)
                *errorOut = i18n("Invalid WEP passphrase.");
            return false;
        }
        s_wsec = NM_SETTING_WIRELESS_SECURITY(nm_setting_wireless_security_new());
        g_object_set(G_OBJECT(s_wsec),
                     NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "none",
                     NM_SETTING_WIRELESS_SECURITY_AUTH_ALG, "open",
                     NM_SETTING_WIRELESS_SECURITY_WEP_KEY_TYPE, wepType,
                     NM_SETTING_WIRELESS_SECURITY_WEP_TX_KEYIDX, 0,
                     NM_SETTING_WIRELESS_SECURITY_WEP_KEY0, secret.utf8().data(),
                     NULL);
        nm_connection_add_setting(conn, NM_SETTING(s_wsec));
        return true;

    case WifiSecLeap:
        if (identity.isEmpty()) {
            if (errorOut)
                *errorOut = i18n("Please enter a username.");
            return false;
        }
        if (secret.isEmpty()) {
            if (errorOut)
                *errorOut = i18n("Please enter a password.");
            return false;
        }
        s_wsec = NM_SETTING_WIRELESS_SECURITY(nm_setting_wireless_security_new());
        g_object_set(G_OBJECT(s_wsec),
                     NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "ieee8021x",
                     NM_SETTING_WIRELESS_SECURITY_AUTH_ALG, "leap",
                     NULL);
        nm_connection_add_setting(conn, NM_SETTING(s_wsec));
        set8021x(conn, eapLeap, identity, secret, NULL);
        return true;

    case WifiSecDynamicWep:
        if (identity.isEmpty()) {
            if (errorOut)
                *errorOut = i18n("Please enter a username.");
            return false;
        }
        if (secret.isEmpty()) {
            if (errorOut)
                *errorOut = i18n("Please enter a password.");
            return false;
        }
        s_wsec = NM_SETTING_WIRELESS_SECURITY(nm_setting_wireless_security_new());
        g_object_set(G_OBJECT(s_wsec),
                     NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "ieee8021x",
                     NM_SETTING_WIRELESS_SECURITY_AUTH_ALG, "open",
                     NULL);
        nm_connection_add_setting(conn, NM_SETTING(s_wsec));
        set8021x(conn, eapPeap, identity, secret, "mschapv2");
        return true;

    case WifiSecWpaPsk:
        if (!nm_utils_wpa_psk_valid(secret.utf8().data())) {
            if (errorOut)
                *errorOut = i18n("Invalid WPA password (8-63 characters).");
            return false;
        }
        s_wsec = NM_SETTING_WIRELESS_SECURITY(nm_setting_wireless_security_new());
        g_object_set(G_OBJECT(s_wsec),
                     NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "wpa-psk",
                     NM_SETTING_WIRELESS_SECURITY_PSK, secret.utf8().data(),
                     NULL);
        nm_connection_add_setting(conn, NM_SETTING(s_wsec));
        return true;

    case WifiSecWpaEnterprise:
        if (identity.isEmpty()) {
            if (errorOut)
                *errorOut = i18n("Please enter a username.");
            return false;
        }
        if (secret.isEmpty()) {
            if (errorOut)
                *errorOut = i18n("Please enter a password.");
            return false;
        }
        s_wsec = NM_SETTING_WIRELESS_SECURITY(nm_setting_wireless_security_new());
        g_object_set(G_OBJECT(s_wsec),
                     NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "wpa-eap",
                     NULL);
        nm_connection_add_setting(conn, NM_SETTING(s_wsec));
        set8021x(conn, eapPeap, identity, secret, "mschapv2");
        return true;

    case WifiSecSae:
        if (!nm_utils_wpa_psk_valid(secret.utf8().data())) {
            if (errorOut)
                *errorOut = i18n("Invalid WPA3 password (8-63 characters).");
            return false;
        }
        s_wsec = NM_SETTING_WIRELESS_SECURITY(nm_setting_wireless_security_new());
        g_object_set(G_OBJECT(s_wsec),
                     NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "sae",
                     NM_SETTING_WIRELESS_SECURITY_PSK, secret.utf8().data(),
                     NULL);
        nm_connection_add_setting(conn, NM_SETTING(s_wsec));
        return true;

    case WifiSecOwe:
        s_wsec = NM_SETTING_WIRELESS_SECURITY(nm_setting_wireless_security_new());
        g_object_set(G_OBJECT(s_wsec),
                     NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "owe",
                     NULL);
        nm_connection_add_setting(conn, NM_SETTING(s_wsec));
        return true;

    default:
        if (errorOut)
            *errorOut = i18n("Unsupported security type.");
        return false;
    }
}
