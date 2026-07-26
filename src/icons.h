#ifndef NM_TRAY_ICONS_H
#define NM_TRAY_ICONS_H

#include <tqstring.h>
#include <tqpixmap.h>

class TQDialog;
class TQWidget;

namespace NmIcons {

TQString signalStrengthIcon(int strength);
TQString deviceStateIcon(int deviceState);
TQString connectingStageIcon(int stage);
TQString notConnectedIcon();
TQString offlineIcon();
TQString disabledTrayIcon();
TQString wiredIcon();
TQString unpluggedIcon();
TQString loopbackIcon();
TQString wirelessIcon();
TQString wirelessOffIcon();
TQString wifiOffIcon();
TQString vpnActiveIcon();
TQString vpnActiveLockIcon();
TQString iconForConnectionType(const char *connectionType);
TQString appIconName();

TQPixmap trayPixmap(const TQString &iconName);
TQPixmap menuPixmap(const TQString &iconName);
TQPixmap windowIconPixmap(const TQString &iconName);
TQPixmap checkMarkPixmap();
TQString notifyIconPath(const TQString &iconName);

void applyDialogIcon(TQDialog *dialog, const TQString &iconName);
int questionYesNo(TQWidget *parent, const TQString &text, const TQString &caption);

} // namespace NmIcons

#endif
