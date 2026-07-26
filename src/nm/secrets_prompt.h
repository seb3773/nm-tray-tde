#ifndef NM_SECRETS_PROMPT_H
#define NM_SECRETS_PROMPT_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *connection_id;
    const char *setting_name;
    const char *secret_key;
    const char *message;
    const char *identity_prefill;
    gboolean ask_identity;
} NmSecretsPromptSpec;

typedef gboolean (*NmSecretsPromptFn)(const NmSecretsPromptSpec *spec,
                                    char **out_identity,
                                    char **out_secret,
                                    gpointer user_data);

void nm_secrets_prompt_set_handler(NmSecretsPromptFn fn, gpointer user_data);

gboolean nm_secrets_prompt_ask2(const NmSecretsPromptSpec *spec,
                                char **out_identity,
                                char **out_secret);

gboolean nm_secrets_prompt_ask(const char *connection_id,
                               const char *setting_name,
                               const char *secret_key,
                               char **out_secret);

#ifdef __cplusplus
}
#endif

#endif
