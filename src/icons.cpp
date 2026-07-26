#include "icons.h"
#include "nm_icons.h"

#include <tdemessagebox.h>

#include <tqdialog.h>
#include <tqimage.h>
#include <tqmessagebox.h>
#include <tqdict.h>

#include <string.h>
#include <unistd.h>

namespace {

struct IconData {
    const unsigned char *data;
    size_t size;
};

static IconData iconDataForName(const TQString &name)
{
    if (name == "check")
        return {check_data, check_size};
    if (name == "nm_device_loopback")
        return {nm_device_loopback_data, nm_device_loopback_size};
    if (name == "nm_device_vpn")
        return {nm_device_vpn_data, nm_device_vpn_size};
    if (name == "nm_device_wired")
        return {nm_device_wired_data, nm_device_wired_size};
    if (name == "nm_not_connected")
        return {nm_not_connected_data, nm_not_connected_size};
    if (name == "nm_connect_stage0")
        return {nm_connect_stage0_data, nm_connect_stage0_size};
    if (name == "nm_connect_stage1")
        return {nm_connect_stage1_data, nm_connect_stage1_size};
    if (name == "nm_connect_stage2")
        return {nm_connect_stage2_data, nm_connect_stage2_size};
    if (name == "nm_connect_stage3")
        return {nm_connect_stage3_data, nm_connect_stage3_size};
    if (name == "nm_connect_stage4")
        return {nm_connect_stage4_data, nm_connect_stage4_size};
    if (name == "nm_connect_stage5")
        return {nm_connect_stage5_data, nm_connect_stage5_size};
    if (name == "nm_connect_stage6")
        return {nm_connect_stage6_data, nm_connect_stage6_size};
    if (name == "nm_signal_0")
        return {nm_signal_0_data, nm_signal_0_size};
    if (name == "nm_signal_25")
        return {nm_signal_25_data, nm_signal_25_size};
    if (name == "nm_signal_50")
        return {nm_signal_50_data, nm_signal_50_size};
    if (name == "nm_signal_75")
        return {nm_signal_75_data, nm_signal_75_size};
    if (name == "nm_signal_100")
        return {nm_signal_100_data, nm_signal_100_size};
    if (name == "nmtde2")
        return {nmtde2_data, nmtde2_size};
    if (name == "unplugged")
        return {unplugged_data, unplugged_size};
    if (name == "wifi_off")
        return {wifi_off_data, wifi_off_size};
    return {0, 0};
}

static TQImage loadEmbeddedPng(const IconData &entry)
{
    TQImage img;
    if (!entry.data || !entry.size)
        return img;
    if (!img.loadFromData(entry.data, (int) entry.size, "PNG"))
        return TQImage();
    if (img.depth() != 32)
        img = img.convertDepth(32);
    img.setAlphaBuffer(true);
    return img;
}

static TQPixmap loadScaled(const IconData &entry, int size)
{
    TQImage img = loadEmbeddedPng(entry);
    if (img.isNull())
        return TQPixmap();
    if (img.width() != size || img.height() != size)
        img = img.smoothScale(size, size);
    if (img.depth() != 32)
        img = img.convertDepth(32);
    img.setAlphaBuffer(true);
    return TQPixmap(img);
}

static TQPixmap cachedScaled(const TQString &cacheKey, const IconData &entry, int size)
{
    static TQDict<TQPixmap> cache;
    TQPixmap *cached = cache.find(cacheKey);
    if (cached)
        return *cached;

    TQPixmap pix = loadScaled(entry, size);
    if (!pix.isNull())
        cache.insert(cacheKey, new TQPixmap(pix));
    return pix;
}

static TQPixmap cachedNative(const TQString &cacheKey, const IconData &entry)
{
    static TQDict<TQPixmap> cache;
    TQPixmap *cached = cache.find(cacheKey);
    if (cached)
        return *cached;

    TQPixmap pix(loadEmbeddedPng(entry));
    if (!pix.isNull())
        cache.insert(cacheKey, new TQPixmap(pix));
    return pix;
}

static const int kTrayIconSize = 22;
static const int kMenuIconSize = 16;

} // namespace

TQString NmIcons::signalStrengthIcon(int strength)
{
    if (strength > 80)
        return "nm_signal_100";
    if (strength > 55)
        return "nm_signal_75";
    if (strength > 30)
        return "nm_signal_50";
    if (strength > 5)
        return "nm_signal_25";
    return "nm_signal_0";
}

TQString NmIcons::deviceStateIcon(int /*deviceState*/)
{
    return connectingStageIcon(0);
}

TQString NmIcons::connectingStageIcon(int stage)
{
    if (stage < 0)
        stage = 0;
    if (stage > 6)
        stage = 6;
    return TQString("nm_connect_stage%1").arg(stage);
}

TQString NmIcons::notConnectedIcon()
{
    return "nm_not_connected";
}

TQString NmIcons::offlineIcon()
{
    return notConnectedIcon();
}

TQString NmIcons::disabledTrayIcon()
{
    return notConnectedIcon();
}

TQString NmIcons::wiredIcon()
{
    return "nm_device_wired";
}

TQString NmIcons::unpluggedIcon()
{
    return "unplugged";
}

TQString NmIcons::loopbackIcon()
{
    return "nm_device_loopback";
}

TQString NmIcons::wirelessIcon()
{
    return "nm_signal_100";
}

TQString NmIcons::wirelessOffIcon()
{
    return "nm_signal_0";
}

TQString NmIcons::wifiOffIcon()
{
    return "wifi_off";
}

TQString NmIcons::vpnActiveIcon()
{
    return "nm_device_vpn";
}

TQString NmIcons::vpnActiveLockIcon()
{
    return vpnActiveIcon();
}

TQString NmIcons::appIconName()
{
    return "nmtde2";
}

TQString NmIcons::iconForConnectionType(const char *connectionType)
{
    if (!connectionType)
        return notConnectedIcon();

    if (strcmp(connectionType, "802-11-wireless") == 0)
        return wirelessIcon();
    if (strcmp(connectionType, "802-3-ethernet") == 0)
        return wiredIcon();
    if (strcmp(connectionType, "loopback") == 0)
        return loopbackIcon();
    if (strcmp(connectionType, "vpn") == 0)
        return vpnActiveIcon();
    if (strcmp(connectionType, "gsm") == 0 || strcmp(connectionType, "cdma") == 0)
        return notConnectedIcon();

    return notConnectedIcon();
}

TQPixmap NmIcons::trayPixmap(const TQString &iconName)
{
    IconData entry = iconDataForName(iconName);
    if (!entry.data)
        entry = iconDataForName(notConnectedIcon());
    return cachedScaled(TQString("tray:") + iconName, entry, kTrayIconSize);
}

TQPixmap NmIcons::menuPixmap(const TQString &iconName)
{
    IconData entry = iconDataForName(iconName);
    if (!entry.data)
        entry = iconDataForName(notConnectedIcon());
    return cachedScaled(TQString("menu:") + iconName, entry, kMenuIconSize);
}

TQPixmap NmIcons::windowIconPixmap(const TQString &iconName)
{
    IconData entry = iconDataForName(iconName);
    if (!entry.data)
        entry = iconDataForName(notConnectedIcon());
    return cachedNative(TQString("win:") + iconName, entry);
}

TQPixmap NmIcons::checkMarkPixmap()
{
    static TQPixmap cached;
    if (!cached.isNull())
        return cached;
    cached = TQPixmap(loadEmbeddedPng({check_data, check_size}));
    return cached;
}

TQString NmIcons::notifyIconPath(const TQString &iconName)
{
    IconData entry = iconDataForName(iconName);
    if (!entry.data)
        entry = iconDataForName(notConnectedIcon());
    if (!entry.data)
        return TQString::null;

    TQString path = TQString("/tmp/nm-tray-tde_%1.png").arg(iconName);
    if (access(path.latin1(), F_OK) != 0) {
        TQImage img = loadEmbeddedPng(entry);
        if (img.isNull())
            return TQString::null;
        if (!img.save(path, "PNG"))
            return TQString::null;
    }
    return path;
}

void NmIcons::applyDialogIcon(TQDialog *dialog, const TQString &iconName)
{
    if (!dialog)
        return;

    TQPixmap pix = windowIconPixmap(iconName);
    if (!pix.isNull())
        dialog->setIcon(pix);
}

int NmIcons::questionYesNo(TQWidget *parent, const TQString &text,
                           const TQString &caption)
{
    TQMessageBox box(caption, text, TQMessageBox::Warning,
                     TQMessageBox::Yes | TQMessageBox::Default,
                     TQMessageBox::No,
                     TQMessageBox::NoButton,
                     parent);
    box.setIcon(TQMessageBox::NoIcon);
    box.setIconPixmap(windowIconPixmap(appIconName()));
    return box.exec() == TQMessageBox::Yes ? KMessageBox::Yes : KMessageBox::No;
}
