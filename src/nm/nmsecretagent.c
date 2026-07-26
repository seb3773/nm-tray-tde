#include "nmsecretagent.h"
#include "secrets_prompt.h"

#include <NetworkManager.h>
#include <nm-secret-agent-old.h>
#include <string.h>

struct _NmSecretAgent {
    NMSecretAgentOld parent;
};

struct _NmSecretAgentClass {
    NMSecretAgentOldClass parent_class;
};

typedef struct _NmSecretAgentClass NmSecretAgentClass;

G_DEFINE_TYPE(NmSecretAgent, nm_secret_agent, NM_TYPE_SECRET_AGENT_OLD)

static void
callback_canceled(NMSecretAgentOld *agent,
                  NMConnection *connection,
                  NMSecretAgentOldGetSecretsFunc callback,
                  gpointer callback_data)
{
    GError *error = g_error_new_literal(NM_SECRET_AGENT_ERROR,
                                        NM_SECRET_AGENT_ERROR_AGENT_CANCELED,
                                        "Secrets request canceled by user");
    callback(agent, connection, NULL, error, callback_data);
    g_error_free(error);
}

static const char *
vpn_message_from_hints(const char **hints)
{
    for (; hints && *hints; ++hints) {
        if (strncmp(*hints, "x-vpn-message:", 14) == 0)
            return *hints + 14;
    }
    return NULL;
}

static gboolean
hints_contain(const char **hints, const char *key)
{
    for (; hints && *hints; ++hints) {
        if (strcmp(*hints, key) == 0)
            return TRUE;
    }
    return FALSE;
}

static const char *
first_secret_hint(const char **hints)
{
    for (; hints && *hints; ++hints) {
        if (strncmp(*hints, "x-", 2) == 0)
            continue;
        return *hints;
    }
    return NULL;
}

static GVariant *
build_secrets_variant(NMConnection *secrets)
{
    GVariant *dict;

    dict = nm_connection_to_dbus(secrets, NM_CONNECTION_SERIALIZE_ONLY_SECRETS);
    return dict;
}

static GVariant *
handle_wireless_security(NMConnection *connection,
                         const char *conn_id,
                         const char **hints)
{
    NMConnection *secrets;
    NMSettingWirelessSecurity *s_wsec;
    NMSettingWirelessSecurity *orig;
    const char *key_mgmt;
    const char *secret_key;
    char *secret = NULL;
    NmSecretsPromptSpec spec;

    orig = nm_connection_get_setting_wireless_security(connection);
    key_mgmt = orig ? nm_setting_wireless_security_get_key_mgmt(orig) : NULL;

    if (key_mgmt && (strcmp(key_mgmt, "wpa-psk") == 0
                     || strcmp(key_mgmt, "sae") == 0
                     || strcmp(key_mgmt, "owe") == 0)) {
        secret_key = "psk";
    } else if (key_mgmt && strcmp(key_mgmt, "wpa-eap") == 0) {
        return NULL;
    } else if (key_mgmt && strcmp(key_mgmt, "ieee8021x") == 0) {
        /* LEAP password lives in 802-1x, not here */
        return NULL;
    } else {
        secret_key = first_secret_hint(hints);
        if (!secret_key)
            secret_key = "wep-key0";
    }

    memset(&spec, 0, sizeof(spec));
    spec.connection_id = conn_id;
    spec.setting_name = NM_SETTING_WIRELESS_SECURITY_SETTING_NAME;
    spec.secret_key = secret_key;

    if (!nm_secrets_prompt_ask2(&spec, NULL, &secret) || !secret)
        return NULL;

    secrets = nm_simple_connection_new();
    s_wsec = NM_SETTING_WIRELESS_SECURITY(nm_setting_wireless_security_new());

    if (strcmp(secret_key, "psk") == 0) {
        g_object_set(G_OBJECT(s_wsec),
                     NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, key_mgmt ? key_mgmt : "wpa-psk",
                     NM_SETTING_WIRELESS_SECURITY_PSK, secret,
                     NULL);
    } else if (strncmp(secret_key, "wep-key", 7) == 0) {
        g_object_set(G_OBJECT(s_wsec),
                     NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "none",
                     NM_SETTING_WIRELESS_SECURITY_WEP_KEY_TYPE,
                     (guint) NM_WEP_KEY_TYPE_PASSPHRASE,
                     NULL);
        nm_setting_wireless_security_set_wep_key(s_wsec, secret_key[7] - '0', secret);
    } else {
        g_object_set(G_OBJECT(s_wsec),
                     NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, key_mgmt ? key_mgmt : "wpa-psk",
                     NM_SETTING_WIRELESS_SECURITY_PSK, secret,
                     NULL);
    }

    nm_connection_add_setting(secrets, NM_SETTING(s_wsec));
    g_free(secret);

    {
        GVariant *dict = build_secrets_variant(secrets);
        g_object_unref(secrets);
        return dict;
    }
}

static GVariant *
handle_802_1x(NMConnection *connection,
              const char *conn_id,
              const char **hints)
{
    NMConnection *secrets;
    NMSetting8021x *s_8021x;
    NMSetting8021x *orig;
    const char *secret_key;
    const char *existing_id;
    char *identity = NULL;
    char *secret = NULL;
    gboolean ask_identity;
    NmSecretsPromptSpec spec;

    orig = nm_connection_get_setting_802_1x(connection);
    existing_id = orig ? nm_setting_802_1x_get_identity(orig) : NULL;

    secret_key = first_secret_hint(hints);
    if (!secret_key)
        secret_key = "password";

    ask_identity = hints_contain(hints, "identity")
        || !existing_id || !existing_id[0];

    memset(&spec, 0, sizeof(spec));
    spec.connection_id = conn_id;
    spec.setting_name = NM_SETTING_802_1X_SETTING_NAME;
    spec.secret_key = secret_key;
    spec.identity_prefill = existing_id;
    spec.ask_identity = ask_identity;

    if (!nm_secrets_prompt_ask2(&spec, ask_identity ? &identity : NULL, &secret)
        || !secret) {
        g_free(identity);
        return NULL;
    }

    secrets = nm_simple_connection_new();
    s_8021x = NM_SETTING_802_1X(nm_setting_802_1x_new());

    if (identity && identity[0]) {
        g_object_set(G_OBJECT(s_8021x),
                     NM_SETTING_802_1X_IDENTITY, identity,
                     NULL);
    }

    if (strcmp(secret_key, "private-key-password") == 0) {
        g_object_set(G_OBJECT(s_8021x),
                     NM_SETTING_802_1X_PRIVATE_KEY_PASSWORD, secret,
                     NULL);
    } else if (strcmp(secret_key, "phase2-private-key-password") == 0) {
        g_object_set(G_OBJECT(s_8021x),
                     NM_SETTING_802_1X_PHASE2_PRIVATE_KEY_PASSWORD, secret,
                     NULL);
    } else if (strcmp(secret_key, "pin") == 0) {
        g_object_set(G_OBJECT(s_8021x),
                     NM_SETTING_802_1X_PIN, secret,
                     NULL);
    } else {
        g_object_set(G_OBJECT(s_8021x),
                     NM_SETTING_802_1X_PASSWORD, secret,
                     NULL);
    }

    nm_connection_add_setting(secrets, NM_SETTING(s_8021x));
    g_free(identity);
    g_free(secret);

    {
        GVariant *dict = build_secrets_variant(secrets);
        g_object_unref(secrets);
        return dict;
    }
}

static GVariant *
handle_vpn(NMConnection *connection,
           const char *conn_id,
           const char **hints)
{
    NMConnection *secrets;
    NMSettingVpn *s_vpn;
    const char *secret_key;
    const char *vpn_message;
    char *secret = NULL;
    NmSecretsPromptSpec spec;

    (void) connection;

    secret_key = first_secret_hint(hints);
    if (!secret_key)
        secret_key = "password";

    vpn_message = vpn_message_from_hints(hints);

    memset(&spec, 0, sizeof(spec));
    spec.connection_id = conn_id;
    spec.setting_name = NM_SETTING_VPN_SETTING_NAME;
    spec.secret_key = secret_key;
    spec.message = vpn_message;

    if (!nm_secrets_prompt_ask2(&spec, NULL, &secret) || !secret)
        return NULL;

    secrets = nm_simple_connection_new();
    s_vpn = NM_SETTING_VPN(nm_setting_vpn_new());
    nm_setting_vpn_add_secret(s_vpn, secret_key, secret);
    nm_connection_add_setting(secrets, NM_SETTING(s_vpn));

    g_free(secret);

    {
        GVariant *dict = build_secrets_variant(secrets);
        g_object_unref(secrets);
        return dict;
    }
}

static GVariant *
handle_pppoe(NMConnection *connection,
             const char *conn_id,
             const char **hints)
{
    NMConnection *secrets;
    NMSettingPppoe *s_pppoe;
    const char *secret_key;
    char *secret = NULL;
    NmSecretsPromptSpec spec;

    (void) connection;

    secret_key = first_secret_hint(hints);
    if (!secret_key)
        secret_key = "password";

    memset(&spec, 0, sizeof(spec));
    spec.connection_id = conn_id;
    spec.setting_name = NM_SETTING_PPPOE_SETTING_NAME;
    spec.secret_key = secret_key;

    if (!nm_secrets_prompt_ask2(&spec, NULL, &secret) || !secret)
        return NULL;

    secrets = nm_simple_connection_new();
    s_pppoe = NM_SETTING_PPPOE(nm_setting_pppoe_new());
    g_object_set(G_OBJECT(s_pppoe),
                 NM_SETTING_PPPOE_PASSWORD, secret,
                 NULL);
    nm_connection_add_setting(secrets, NM_SETTING(s_pppoe));
    g_free(secret);

    {
        GVariant *dict = build_secrets_variant(secrets);
        g_object_unref(secrets);
        return dict;
    }
}

static void
get_secrets(NMSecretAgentOld *agent,
            NMConnection *connection,
            const char *connection_path,
            const char *setting_name,
            const char **hints,
            guint32 flags,
            NMSecretAgentOldGetSecretsFunc callback,
            gpointer callback_data)
{
    GError *error = NULL;
    const char *conn_id;
    NMSetting *setting;
    GVariant *secrets = NULL;

    (void) connection_path;
    (void) flags;

    if (!connection || !setting_name) {
        error = g_error_new(NM_SECRET_AGENT_ERROR,
                            NM_SECRET_AGENT_ERROR_INVALID_CONNECTION,
                            "Invalid secrets request");
        callback(agent, connection, NULL, error, callback_data);
        g_error_free(error);
        return;
    }

    setting = nm_connection_get_setting_by_name(connection, setting_name);
    if (!setting) {
        error = g_error_new(NM_SECRET_AGENT_ERROR,
                            NM_SECRET_AGENT_ERROR_INVALID_CONNECTION,
                            "Connection has no setting '%s'",
                            setting_name);
        callback(agent, connection, NULL, error, callback_data);
        g_error_free(error);
        return;
    }

    conn_id = nm_connection_get_id(connection);
    if (!conn_id)
        conn_id = "Network";

    if (strcmp(setting_name, NM_SETTING_WIRELESS_SECURITY_SETTING_NAME) == 0)
        secrets = handle_wireless_security(connection, conn_id, hints);
    else if (strcmp(setting_name, NM_SETTING_802_1X_SETTING_NAME) == 0)
        secrets = handle_802_1x(connection, conn_id, hints);
    else if (strcmp(setting_name, NM_SETTING_VPN_SETTING_NAME) == 0)
        secrets = handle_vpn(connection, conn_id, hints);
    else if (strcmp(setting_name, NM_SETTING_PPPOE_SETTING_NAME) == 0)
        secrets = handle_pppoe(connection, conn_id, hints);
    else {
        error = g_error_new(NM_SECRET_AGENT_ERROR,
                            NM_SECRET_AGENT_ERROR_NO_SECRETS,
                            "nm-tray-tde: unsupported secret request for setting '%s'",
                            setting_name);
        callback(agent, connection, NULL, error, callback_data);
        g_error_free(error);
        return;
    }

    if (!secrets) {
        callback_canceled(agent, connection, callback, callback_data);
        return;
    }

    callback(agent, connection, secrets, NULL, callback_data);
    g_variant_unref(secrets);
}

static void
cancel_get_secrets(NMSecretAgentOld *agent,
                   const char *connection_path,
                   const char *setting_name)
{
    (void) agent;
    (void) connection_path;
    (void) setting_name;
}

static void
save_secrets(NMSecretAgentOld *agent,
             NMConnection *connection,
             const char *connection_path,
             NMSecretAgentOldSaveSecretsFunc callback,
             gpointer callback_data)
{
    (void) connection_path;
    callback(agent, connection, NULL, callback_data);
}

static void
delete_secrets(NMSecretAgentOld *agent,
               NMConnection *connection,
               const char *connection_path,
               NMSecretAgentOldDeleteSecretsFunc callback,
               gpointer callback_data)
{
    (void) connection_path;
    callback(agent, connection, NULL, callback_data);
}

static void
nm_secret_agent_init(NmSecretAgent *self)
{
    (void) self;
}

static void
nm_secret_agent_class_init(NmSecretAgentClass *klass)
{
    NMSecretAgentOldClass *parent = NM_SECRET_AGENT_OLD_CLASS(klass);

    parent->get_secrets = get_secrets;
    parent->cancel_get_secrets = cancel_get_secrets;
    parent->save_secrets = save_secrets;
    parent->delete_secrets = delete_secrets;
}

NmSecretAgent *
nm_secret_agent_create(GError **error)
{
    NmSecretAgent *agent;

    agent = g_object_new(nm_secret_agent_get_type(),
                         NM_SECRET_AGENT_OLD_IDENTIFIER, "nm-tray-tde",
                         NM_SECRET_AGENT_OLD_AUTO_REGISTER, TRUE,
                         NM_SECRET_AGENT_OLD_CAPABILITIES,
                         (guint) NM_SECRET_AGENT_CAPABILITY_VPN_HINTS,
                         NULL);

    if (!g_initable_init(G_INITABLE(agent), NULL, error)) {
        g_object_unref(agent);
        return NULL;
    }

    return agent;
}

void
nm_secret_agent_destroy(NmSecretAgent *agent)
{
    if (agent)
        g_object_unref(agent);
}
