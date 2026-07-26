#ifndef IPV4_CONFIG_H
#define IPV4_CONFIG_H

#include <tqstring.h>

typedef struct _NMConnection NMConnection;

struct Ipv4EditorState {
    bool manual;
    TQString address;
    TQString netmask;
    TQString gateway;
    TQString dns;
    TQString dnsSearch;
};

void ipv4EditorLoad(NMConnection *conn, Ipv4EditorState *state);
bool ipv4EditorApply(NMConnection *conn, const Ipv4EditorState &state, TQString *errorOut);

#endif
