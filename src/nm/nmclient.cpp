#include "nmclient.h"
#ifndef NM_NO_SECRET_AGENT
#include "nmsecretagent.h"
#endif
#include "nmeventpump.h"
#include "glib_compat.h"

#include <tqmap.h>

#include <stdio.h>
#include <string.h>

struct DeviceSignals {
    gulong stateChanged;
    gulong apAdded;
    gulong apRemoved;
    gulong lastScan;
    DeviceSignals() : stateChanged(0), apAdded(0), apRemoved(0), lastScan(0) {}
};

typedef TQMap<void *, DeviceSignals> DeviceSignalMap;

static void onClientNotify(GObject *obj, GParamSpec *pspec, gpointer user_data)
{
    (void) obj;
    (void) pspec;
    NmClient *self = static_cast<NmClient *>(user_data);
    if (self)
        self->notifyChanged();
}

static void onDeviceAdded(NMClient *client, NMDevice *device, gpointer user_data)
{
    (void) client;
    NmClient *self = static_cast<NmClient *>(user_data);
    if (self && device)
        self->connectDevice(device);
    if (self)
        self->notifyChanged();
}

static void onDeviceRemoved(NMClient *client, NMDevice *device, gpointer user_data)
{
    (void) client;
    NmClient *self = static_cast<NmClient *>(user_data);
    if (self && device)
        self->disconnectDevice(device);
    if (self)
        self->notifyChanged();
}

static void onConnectionAdded(NMClient *client, NMRemoteConnection *connection, gpointer user_data)
{
    (void) client;
    (void) connection;
    NmClient *self = static_cast<NmClient *>(user_data);
    if (self)
        self->notifyChanged();
}

static void onConnectionRemoved(NMClient *client, NMRemoteConnection *connection, gpointer user_data)
{
    (void) client;
    (void) connection;
    NmClient *self = static_cast<NmClient *>(user_data);
    if (self)
        self->notifyChanged();
}

static void onDeviceStateChanged(NMDevice *device, NMDeviceState new_state,
                                 NMDeviceState old_state, NMDeviceStateReason reason,
                                 gpointer user_data)
{
    (void) device;
    (void) new_state;
    (void) old_state;
    (void) reason;
    NmClient *self = static_cast<NmClient *>(user_data);
    if (self)
        self->notifyChanged();
}

static void onAccessPointAdded(NMDeviceWifi *device, NMAccessPoint *ap, gpointer user_data)
{
    (void) device;
    (void) ap;
    NmClient *self = static_cast<NmClient *>(user_data);
    if (self)
        self->notifyChanged();
}

static void onAccessPointRemoved(NMDeviceWifi *device, NMAccessPoint *ap, gpointer user_data)
{
    (void) device;
    (void) ap;
    NmClient *self = static_cast<NmClient *>(user_data);
    if (self)
        self->notifyChanged();
}

NmClient::NmClient(TQObject *parent)
    : TQObject(parent)
    , m_client(0)
    , m_secretAgent(0)
    , m_deviceSignals(new DeviceSignalMap())
{
}

NmClient::~NmClient()
{
    shutdown();
    delete static_cast<DeviceSignalMap *>(m_deviceSignals);
    m_deviceSignals = 0;
}

void NmClient::emitChanged()
{
    emit changed();
}

void NmClient::notifyChanged()
{
    emitChanged();
}

void NmClient::connectDevice(void *devicePtr)
{
    NMDevice *device = static_cast<NMDevice *>(devicePtr);
    DeviceSignalMap *map = static_cast<DeviceSignalMap *>(m_deviceSignals);

    if (!device || map->contains(device))
        return;

    DeviceSignals sigs;
    sigs.stateChanged = g_signal_connect(device, "state-changed",
                                         G_CALLBACK(onDeviceStateChanged), this);

    if (NM_IS_DEVICE_WIFI(device)) {
        NMDeviceWifi *wifi = NM_DEVICE_WIFI(device);
        sigs.apAdded = g_signal_connect(wifi, "access-point-added",
                                       G_CALLBACK(onAccessPointAdded), this);
        sigs.apRemoved = g_signal_connect(wifi, "access-point-removed",
                                          G_CALLBACK(onAccessPointRemoved), this);
        sigs.lastScan = g_signal_connect(wifi, "notify::last-scan",
                                         G_CALLBACK(onClientNotify), this);
    }

    map->insert(device, sigs);
}

void NmClient::disconnectDevice(void *devicePtr)
{
    NMDevice *device = static_cast<NMDevice *>(devicePtr);
    DeviceSignalMap *map = static_cast<DeviceSignalMap *>(m_deviceSignals);

    if (!device || !map->contains(device))
        return;

    DeviceSignals sigs = (*map)[device];
    if (sigs.stateChanged)
        g_signal_handler_disconnect(device, sigs.stateChanged);
    if (NM_IS_DEVICE_WIFI(device)) {
        NMDeviceWifi *wifi = NM_DEVICE_WIFI(device);
        if (sigs.apAdded)
            g_signal_handler_disconnect(wifi, sigs.apAdded);
        if (sigs.apRemoved)
            g_signal_handler_disconnect(wifi, sigs.apRemoved);
        if (sigs.lastScan)
            g_signal_handler_disconnect(wifi, sigs.lastScan);
    }
    map->remove(device);
}

void NmClient::connectClientSignals()
{
    g_signal_connect(m_client, "notify::state", G_CALLBACK(onClientNotify), this);
    g_signal_connect(m_client, "notify::active-connections", G_CALLBACK(onClientNotify), this);
    g_signal_connect(m_client, "notify::wireless-enabled", G_CALLBACK(onClientNotify), this);
    g_signal_connect(m_client, "notify::wwan-enabled", G_CALLBACK(onClientNotify), this);
    g_signal_connect(m_client, "notify::manager-running", G_CALLBACK(onClientNotify), this);
    g_signal_connect(m_client, "notify::connectivity", G_CALLBACK(onClientNotify), this);
    g_signal_connect(m_client, "device-added", G_CALLBACK(onDeviceAdded), this);
    g_signal_connect(m_client, "device-removed", G_CALLBACK(onDeviceRemoved), this);
    g_signal_connect(m_client, "connection-added", G_CALLBACK(onConnectionAdded), this);
    g_signal_connect(m_client, "connection-removed", G_CALLBACK(onConnectionRemoved), this);

    const GPtrArray *devices = nm_client_get_devices(m_client);
    if (devices) {
        for (guint i = 0; i < devices->len; ++i)
            connectDevice(g_ptr_array_index(devices, i));
    }
}

bool NmClient::init(TQString *errorOut, bool enableSecretAgent)
{
    if (m_client)
        return true;

    GError *error = 0;
    m_client = nm_client_new(NULL, &error);
    if (!m_client) {
        if (errorOut) {
            if (error)
                *errorOut = TQString::fromUtf8(error->message);
            else
                *errorOut = TQString::fromLatin1("nm_client_new() failed");
        }
        if (error)
            g_error_free(error);
        return false;
    }

    if (enableSecretAgent) {
#ifndef NM_NO_SECRET_AGENT
        m_secretAgent = nm_secret_agent_create(&error);
        if (!m_secretAgent) {
            if (errorOut) {
                if (error)
                    *errorOut = TQString::fromUtf8(error->message);
                else
                    *errorOut = TQString::fromLatin1("nm_secret_agent_create() failed");
            }
            if (error)
                g_error_free(error);
            shutdown();
            return false;
        }
#else
        if (errorOut)
            *errorOut = TQString::fromLatin1("secret agent not available");
        shutdown();
        return false;
#endif
    }

    connectClientSignals();
    return true;
}

void NmClient::shutdown()
{
    DeviceSignalMap *map = static_cast<DeviceSignalMap *>(m_deviceSignals);
    if (map) {
        while (!map->isEmpty())
            disconnectDevice(map->begin().key());
    }

    if (m_secretAgent) {
#ifndef NM_NO_SECRET_AGENT
        nm_secret_agent_destroy(static_cast<NmSecretAgent *>(m_secretAgent));
#endif
        m_secretAgent = 0;
    }

    if (m_client) {
        g_signal_handlers_disconnect_by_data(m_client, this);
        g_object_unref(m_client);
        m_client = 0;
    }
}

bool NmClient::isNmRunning() const
{
    return m_client ? nm_client_get_nm_running(m_client) : false;
}

int NmClient::clientState() const
{
    return m_client ? (int) nm_client_get_state(m_client) : (int) NM_STATE_UNKNOWN;
}

bool NmClient::networkingEnabled() const
{
    return m_client ? nm_client_networking_get_enabled(m_client) : false;
}

bool NmClient::wirelessEnabled() const
{
    return m_client ? nm_client_wireless_get_enabled(m_client) : false;
}

bool NmClient::wiredEnabled() const
{
    if (!m_client)
        return false;
    const GPtrArray *devices = nm_client_get_devices(m_client);
    for (guint i = 0; devices && i < devices->len; ++i) {
        NMDevice *d = NM_DEVICE(g_ptr_array_index(devices, i));
        if (NM_IS_DEVICE_ETHERNET(d) && !nm_device_is_software(d)) {
            if (nm_device_get_managed(d))
                return true;
        }
    }
    return false;
}

bool NmClient::wirelessHardwareEnabled() const
{
    return m_client ? nm_client_wireless_hardware_get_enabled(m_client) : false;
}

int NmClient::connectivity() const
{
    return m_client ? (int) nm_client_get_connectivity(m_client)
                    : (int) NM_CONNECTIVITY_UNKNOWN;
}

bool NmClient::setNetworkingEnabled(bool enabled)
{
    if (!m_client)
        return false;
    GError *error = 0;
    if (!nm_client_networking_set_enabled(m_client, enabled ? TRUE : FALSE, &error)) {
        if (error)
            g_error_free(error);
        return false;
    }
    NmEventPump::pumpRepeated(10);
    return true;
}

bool NmClient::setWirelessEnabled(bool enabled)
{
    if (!m_client)
        return false;
    nm_client_wireless_set_enabled(m_client, enabled ? TRUE : FALSE);
    NmEventPump::pumpRepeated(10);
    return true;
}

bool NmClient::setWiredEnabled(bool enabled)
{
    if (!m_client)
        return false;
    const GPtrArray *devices = nm_client_get_devices(m_client);
    for (guint i = 0; devices && i < devices->len; ++i) {
        NMDevice *d = NM_DEVICE(g_ptr_array_index(devices, i));
        if (NM_IS_DEVICE_ETHERNET(d) && !nm_device_is_software(d)) {
            // Setting the device as managed allows NetworkManager to naturally
            // apply its own auto-connect policies (which depend on carrier/cable presence).
            // We MUST NOT force an activation here.
            nm_device_set_managed(d, enabled ? TRUE : FALSE);
            
            if (!enabled) {
                // Ensure it is properly disconnected if disabling
                nm_device_disconnect_async(d, NULL, NULL, NULL);
            }
        }
    }
    return true;
}

bool NmClient::activateConnection(NMConnection *connection, NMDevice *device,
                                  const TQString &specificObject)
{
    if (!m_client || !connection || !device)
        return false;

    const char *specific = "/";
    TQCString specificUtf8;
    if (!specificObject.isNull() && !specificObject.isEmpty()) {
        specificUtf8 = specificObject.utf8();
        specific = specificUtf8.data();
    }

    nm_client_activate_connection_async(m_client, connection, device, specific,
                                        NULL, NULL, NULL);
    NmEventPump::pumpAfterAsync();
    return true;
}

bool NmClient::activateConnection(const TQString &connectionPath,
                                  const TQString &devicePath,
                                  const TQString &specificObject)
{
    if (!m_client)
        return false;

    TQCString connPathUtf8 = connectionPath.utf8();
    NMRemoteConnection *conn = nm_client_get_connection_by_path(
        m_client, connPathUtf8.data());
        
    TQCString devPathUtf8 = devicePath.utf8();
    NMDevice *device = nm_client_get_device_by_path(
        m_client, devPathUtf8.data());
    if (!conn || !device)
        return false;

    return activateConnection(NM_CONNECTION(conn), device, specificObject);
}

bool NmClient::deactivateDevice(const TQString &devicePath)
{
    if (!m_client)
        return false;

    TQCString devPathUtf8 = devicePath.utf8();
    NMDevice *device = nm_client_get_device_by_path(
        m_client, devPathUtf8.data());
    if (!device)
        return false;

    nm_device_disconnect_async(device, NULL, NULL, NULL);
    NmEventPump::pumpAfterAsync();
    return true;
}

bool NmClient::deactivateActiveConnection(const TQString &uuidOrConnectionPath)
{
    if (!m_client || uuidOrConnectionPath.isEmpty())
        return false;

    const GPtrArray *connections = nm_client_get_active_connections(m_client);
    for (guint i = 0; connections && i < connections->len; ++i) {
        NMActiveConnection *active = (NMActiveConnection *) g_ptr_array_index(connections, i);
        const char *uuid = nm_active_connection_get_uuid(active);
        NMConnection *conn = NM_CONNECTION(nm_active_connection_get_connection(active));
        const char *path = conn ? nm_connection_get_path(conn) : 0;

        if ((uuid && uuidOrConnectionPath == TQString::fromUtf8(uuid))
            || (path && uuidOrConnectionPath == TQString::fromUtf8(path))) {
            nm_client_deactivate_connection_async(m_client, active, NULL, NULL, NULL);
            NmEventPump::pumpAfterAsync();
            return true;
        }
    }

    return false;
}

bool NmClient::deactivateWifiConnections()
{
    return deactivateWifiOnDevice(0);
}

bool NmClient::deactivateWifiOnDevice(NMDevice *device)
{
    bool any = false;

    if (!m_client)
        return false;

    const GPtrArray *connections = nm_client_get_active_connections(m_client);
    for (guint i = 0; connections && i < connections->len; ++i) {
        NMActiveConnection *active = (NMActiveConnection *) g_ptr_array_index(connections, i);
        NMActiveConnectionState state = nm_active_connection_get_state(active);
        const GPtrArray *devices;
        NMDevice *activeDev = 0;
        NMConnection *conn;
        NMSettingConnection *s_con;
        const char *ctype;

        if (state != NM_ACTIVE_CONNECTION_STATE_ACTIVATED
            && state != NM_ACTIVE_CONNECTION_STATE_ACTIVATING)
            continue;

        devices = nm_active_connection_get_devices(active);
        if (devices && devices->len)
            activeDev = (NMDevice *) g_ptr_array_index(devices, 0);

        if (activeDev) {
            if (!NM_IS_DEVICE_WIFI(activeDev))
                continue;
            if (device && activeDev != device)
                continue;
        } else {
            /* Activating profiles may not expose a device yet. */
            conn = NM_CONNECTION(nm_active_connection_get_connection(active));
            s_con = conn ? nm_connection_get_setting_connection(conn) : 0;
            ctype = s_con ? nm_setting_connection_get_connection_type(s_con) : 0;
            if (!ctype || strcmp(ctype, NM_SETTING_WIRELESS_SETTING_NAME) != 0)
                continue;
        }

        nm_client_deactivate_connection_async(m_client, active, NULL, NULL, NULL);
        any = true;
    }

    if (any)
        NmEventPump::pumpAfterAsync();
    return any;
}

void NmClient::abortActivatingConnections(bool isWifi)
{
    if (!m_client)
        return;

    const GPtrArray *connections = nm_client_get_active_connections(m_client);
    for (guint i = 0; connections && i < connections->len; ++i) {
        NMActiveConnection *ac = NM_ACTIVE_CONNECTION(g_ptr_array_index(connections, i));
        if (nm_active_connection_get_state(ac) == NM_ACTIVE_CONNECTION_STATE_ACTIVATING) {
            const GPtrArray *devices = nm_active_connection_get_devices(ac);
            NMDevice *device = 0;
            if (devices && devices->len)
                device = NM_DEVICE(g_ptr_array_index(devices, 0));

            bool acIsWifi = (device && NM_IS_DEVICE_WIFI(device));
            bool acIsWired = (device && NM_IS_DEVICE_ETHERNET(device));

            // Only abort if it matches the target device type
            if ((isWifi && acIsWifi) || (!isWifi && acIsWired)) {
                nm_client_deactivate_connection(m_client, ac, NULL, NULL);
            }
        }
    }
}

bool NmClient::requestWifiScan(const TQString &devicePath)
{
    if (!m_client)
        return false;

    /* devicePath must be the D-Bus object path (nm_object_get_path),
     * not nm_device_get_path() which returns the udev sysfs path. */
    TQCString devPathUtf8 = devicePath.utf8();
    NMDevice *device = nm_client_get_device_by_path(
        m_client, devPathUtf8.data());
    if (!device || !NM_IS_DEVICE_WIFI(device))
        return false;

    NMDeviceWifi *wifi = NM_DEVICE_WIFI(device);
    GError *error = 0;

    /* Same as nm-applet: kick the scan (returns before scan finishes).
     * Completion is observed via notify::last-scan / access-point-added. */
    if (!nm_device_wifi_request_scan(wifi, NULL, &error)) {
        if (error) {
            const char *msg = error->message;
            bool softFail = msg && (
                strstr(msg, "already") || strstr(msg, "Already")
                || strstr(msg, "in progress") || strstr(msg, "In progress")
                || strstr(msg, "not allowed") || strstr(msg, "Not authorized"));
            g_error_free(error);
            NmEventPump::pumpRepeated(5);
            return softFail;
        }
        return false;
    }

    NmEventPump::pumpRepeated(5);
    return true;
}

static gint64 wifiDeviceLastScanMs(NMDeviceWifi *wifi)
{
    gint64 lastScan = nm_device_wifi_get_last_scan(wifi);

    if (lastScan == -1)
        return G_MININT64;
    return lastScan;
}

static bool wifiScanProgressed(NMDeviceWifi *wifi, gint64 rescanCutoffMs,
                               guint apCountBefore)
{
    const GPtrArray *aps = nm_device_wifi_get_access_points(wifi);
    guint apCount = aps ? aps->len : 0;
    gint64 lastScan = wifiDeviceLastScanMs(wifi);

    /* nmcli-style: scan is done when LastScan advances past the request time. */
    if (lastScan < rescanCutoffMs)
        return false;

    if (apCount > apCountBefore || apCount > 1)
        return true;

    /* LastScan moved but APs not published yet — caller keeps waiting. */
    return false;
}

bool NmClient::requestWifiScanAndWait(const TQString &devicePath, int timeoutMs)
{
    if (!m_client)
        return false;

    TQCString devPathUtf8 = devicePath.utf8();
    NMDevice *device = nm_client_get_device_by_path(
        m_client, devPathUtf8.data());
    if (!device || !NM_IS_DEVICE_WIFI(device))
        return false;

    NMDeviceWifi *wifi = NM_DEVICE_WIFI(device);
    const GPtrArray *aps = nm_device_wifi_get_access_points(wifi);
    guint apCountBefore = aps ? aps->len : 0;
    gint64 rescanCutoffMs = nm_utils_get_timestamp_msec();

    nm_device_wifi_request_scan(wifi, NULL, NULL);

    if (timeoutMs <= 0) {
        NmEventPump::pump();
        return true;
    }

    gint64 deadline = g_get_monotonic_time() + (gint64) timeoutMs * 1000;
    while (g_get_monotonic_time() < deadline) {
        NmEventPump::pumpUi();

        if (wifiScanProgressed(wifi, rescanCutoffMs, apCountBefore)) {
            NmEventPump::pumpUi();
            return true;
        }

        g_usleep(20000);
    }

    NmEventPump::pumpUi();
    return wifiScanProgressed(wifi, rescanCutoffMs, apCountBefore);
}

void NmClient::dumpStatus() const
{
    if (!m_client) {
        printf("NMClient: not initialized\n");
        return;
    }

    printf("NetworkManager running : %s\n", isNmRunning() ? "yes" : "no");
    printf("Client state         : %d\n", clientState());
    printf("Networking enabled   : %s\n", networkingEnabled() ? "yes" : "no");
    printf("Wireless enabled     : %s\n", wirelessEnabled() ? "yes" : "no");

    const GPtrArray *devices = nm_client_get_devices(m_client);
    if (devices) {
        printf("Devices (%u):\n", devices->len);
        for (guint i = 0; i < devices->len; ++i) {
            NMDevice *dev = (NMDevice *) g_ptr_array_index(devices, i);
            printf("  - %s (%s) state=%d\n",
                   nm_device_get_iface(dev),
                   nm_device_get_type_description(dev),
                   (int) nm_device_get_state(dev));
        }
    }

    const GPtrArray *connections = nm_client_get_connections(m_client);
    if (connections) {
        printf("Saved connections (%u):\n", connections->len);
        for (guint i = 0; i < connections->len; ++i) {
            NMRemoteConnection *conn = (NMRemoteConnection *) g_ptr_array_index(connections, i);
            NMSettingConnection *s_con = nm_connection_get_setting_connection(NM_CONNECTION(conn));
            if (!s_con)
                continue;
            printf("  - %s (%s)\n",
                   nm_setting_connection_get_id(s_con),
                   nm_setting_connection_get_uuid(s_con));
        }
    }

    const GPtrArray *active = nm_client_get_active_connections(m_client);
    if (active) {
        printf("Active connections (%u):\n", active->len);
        for (guint i = 0; i < active->len; ++i) {
            NMActiveConnection *ac = (NMActiveConnection *) g_ptr_array_index(active, i);
            printf("  - %s state=%d\n",
                   nm_active_connection_get_id(ac),
                   (int) nm_active_connection_get_state(ac));
        }
    }
}

#include "nmclient.moc"
