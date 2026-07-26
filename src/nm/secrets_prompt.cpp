#include "secrets_prompt.h"

#include "../secretsdialog.h"

#include <NetworkManager.h>
#include <tdelocale.h>
#include <ntqstring.h>

#include <string.h>

static NmSecretsPromptFn s_promptFn = 0;
static gpointer s_promptUserData = 0;

static TQString secretKeyLabel(const char *setting_name, const char *secret_key)
{
    if (!secret_key)
        return i18n("Password:");

    if (strcmp(secret_key, "psk") == 0)
        return i18n("Wi-Fi password:");
    if (strcmp(secret_key, "identity") == 0)
        return i18n("Username:");
    if (strncmp(secret_key, "wep-key", 7) == 0)
        return i18n("WEP key:");
    if (strcmp(secret_key, "password") == 0 || strcmp(secret_key, "passwd") == 0)
        return i18n("Password:");
    if (strcmp(secret_key, "private-key-password") == 0)
        return i18n("Private key password:");
    if (strcmp(secret_key, "phase2-private-key-password") == 0)
        return i18n("Phase 2 private key password:");
    if (strcmp(secret_key, "pin") == 0)
        return i18n("PIN:");
    if (strcmp(secret_key, "secret") == 0 || strcmp(secret_key, "ipsec-secret") == 0)
        return i18n("VPN secret:");

    if (setting_name && strcmp(setting_name, NM_SETTING_VPN_SETTING_NAME) == 0)
        return i18n("VPN password:");

    return i18n("Password:");
}

extern "C" void nm_secrets_prompt_set_handler(NmSecretsPromptFn fn, gpointer user_data)
{
    s_promptFn = fn;
    s_promptUserData = user_data;
}

extern "C" gboolean nm_secrets_prompt_ask2(const NmSecretsPromptSpec *spec,
                                           char **out_identity,
                                           char **out_secret)
{
    if (!spec || !out_secret)
        return FALSE;

    if (out_identity)
        *out_identity = 0;
    *out_secret = 0;

    if (s_promptFn)
        return s_promptFn(spec, out_identity, out_secret, s_promptUserData);

    SecretsDialogRequest req;
    req.title = TQString::fromUtf8(spec->connection_id ? spec->connection_id : "Network");
    req.message = spec->message ? TQString::fromUtf8(spec->message) : TQString::null;
    req.showIdentity = spec->ask_identity ? true : false;
    req.identityLabel = i18n("Username:");
    req.identityPrefill = spec->identity_prefill
        ? TQString::fromUtf8(spec->identity_prefill) : TQString::null;
    req.secretLabel = secretKeyLabel(spec->setting_name, spec->secret_key);

    SecretsDialogResult result;
    if (!runSecretsDialog(req, &result) || !result.accepted)
        return FALSE;

    if (out_identity && spec->ask_identity && !result.identity.isEmpty())
        *out_identity = g_strdup(result.identity.utf8().data());

    *out_secret = g_strdup(result.secret.utf8().data());
    return TRUE;
}

extern "C" gboolean nm_secrets_prompt_ask(const char *connection_id,
                                          const char *setting_name,
                                          const char *secret_key,
                                          char **out_secret)
{
    NmSecretsPromptSpec spec;

    memset(&spec, 0, sizeof(spec));
    spec.connection_id = connection_id;
    spec.setting_name = setting_name;
    spec.secret_key = secret_key;
    spec.ask_identity = FALSE;

    return nm_secrets_prompt_ask2(&spec, 0, out_secret);
}
