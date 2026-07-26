#include "nm-probe.h"
#include "../src/nm/nmclient.h"
#include "../src/nm/glib_compat.h"
#include "../src/nm/nmeventpump.h"

#include <tqapplication.h>
#include <tqtimer.h>

#include <stdio.h>
#include <stdlib.h>

ProbeWatcher::ProbeWatcher(NmClient *client, TQObject *parent)
    : TQObject(parent)
    , m_client(client)
{
}

void ProbeWatcher::refresh()
{
    printf("\n--- refresh ---\n");
    m_client->dumpStatus();
}

int main(int argc, char *argv[])
{
    TQApplication app(argc, argv, false);

    NmClient client;
    TQString error;
    if (!client.init(&error, false)) {
        fprintf(stderr, "nm-probe: %s\n", error.local8Bit().data());
        return 1;
    }

    client.dumpStatus();

    if (argc > 1 && TQString(argv[1]) == "--watch") {
        printf("\nWatching NM (Ctrl+C to stop)...\n");
        NmEventPump pump(500);
        ProbeWatcher watcher(&client);
        TQTimer refresh;
        TQObject::connect(&refresh, TQT_SIGNAL(timeout()), &watcher, TQT_SLOT(refresh()));
        refresh.start(5000);
        return app.exec();
    }

    return 0;
}

#include "nm-probe.moc"
