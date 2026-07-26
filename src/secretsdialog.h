#ifndef SECRETS_DIALOG_H
#define SECRETS_DIALOG_H

#include <tqstring.h>

struct SecretsDialogRequest {
    TQString title;
    TQString message;
    bool showIdentity;
    TQString identityLabel;
    TQString identityPrefill;
    TQString secretLabel;
};

struct SecretsDialogResult {
    TQString identity;
    TQString secret;
    bool accepted;
};

bool runSecretsDialog(const SecretsDialogRequest &request,
                      SecretsDialogResult *result);

#endif
