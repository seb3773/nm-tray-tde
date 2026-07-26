#ifndef WIFI_UTIL_H
#define WIFI_UTIL_H

#include <tqstring.h>

typedef struct _GBytes GBytes;
typedef struct _NMClient NMClient;
typedef struct _NMDevice NMDevice;
typedef struct _NMDeviceWifi NMDeviceWifi;
typedef struct _NMAccessPoint NMAccessPoint;
typedef struct _NMRemoteConnection NMRemoteConnection;

TQString ssidToString(GBytes *ssid);
bool ssidsMatch(const TQString &ssid, const TQString &profileSsid);
bool isEmptySsidBytes(GBytes *ssid);
bool isSystemVirtualIfaceName(const char *iface);
bool isRelevantDevice(NMDevice *device);
bool isMenuVisibleDevice(NMDevice *device);

NMDeviceWifi *pickPrimaryWifiDevice(NMClient *client);
NMDeviceWifi *pickApCapableWifiDevice(NMClient *client);
NMAccessPoint *accessPointForSsid(NMDeviceWifi *wifi, const TQString &ssid);
NMRemoteConnection *savedConnectionForAccessPoint(NMClient *client,
                                                  NMDevice *device,
                                                  NMAccessPoint *ap);
NMRemoteConnection *savedConnectionForSsidOnDevice(NMDevice *device,
                                                   const TQString &ssid);

#endif
