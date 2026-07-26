#ifndef CONNECTION_INFO_BUILDER_H
#define CONNECTION_INFO_BUILDER_H

#include <tqstring.h>
#include <tqvaluelist.h>

typedef struct _NMClient NMClient;

struct ConnectionInfoEntry {
    TQString tabTitle;
    TQString body;
    TQString iconName;
};

TQValueList<ConnectionInfoEntry> buildActiveConnectionInfoEntries(NMClient *client);
bool hasActiveConnectionInfo(NMClient *client);

#endif
