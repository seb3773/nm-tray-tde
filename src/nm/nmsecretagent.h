#ifndef NM_SECRET_AGENT_H
#define NM_SECRET_AGENT_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _NmSecretAgent NmSecretAgent;

NmSecretAgent *nm_secret_agent_create(GError **error);
void nm_secret_agent_destroy(NmSecretAgent *agent);

#ifdef __cplusplus
}
#endif

#endif
