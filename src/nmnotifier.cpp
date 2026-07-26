#include "nmnotifier.h"
#include "nmnotifier_glue.h"
#include "nmdata.h"
#include "icons.h"

#include <tdelocale.h>
#include <tdeconfig.h>

#include <unistd.h>

NmNotifier::NmNotifier()
    : m_initialized(false)
    , m_enabled(true)
    , m_seeded(false)
    , m_hadConnection(false)
{
    TDEConfig config("nmtrayrc");
    config.setGroup("General");
    m_enabled = config.readBoolEntry("EnableNotifications", true);
}

NmNotifier::~NmNotifier()
{
    shutdown();
}

bool NmNotifier::init()
{
    if (m_initialized)
        return true;

    if (!nm_notifier_glue_init("nm-tray-tde"))
        return false;

    m_initialized = true;
    return true;
}

void NmNotifier::shutdown()
{
    if (!m_initialized)
        return;

    nm_notifier_glue_shutdown();
    m_initialized = false;
}

void NmNotifier::setEnabled(bool enabled)
{
    m_enabled = enabled;

    TDEConfig config("nmtrayrc");
    config.setGroup("General");
    config.writeEntry("EnableNotifications", enabled);
    config.sync();
}

void NmNotifier::showNotification(const TQString &summary, const TQString &body,
                                  const TQString &iconName)
{
    const char *iconFile = NULL;
    TQCString iconPath;

    if (!m_initialized || !m_enabled)
        return;

    if (!iconName.isEmpty()) {
        TQString path = NmIcons::notifyIconPath(iconName);
        if (!path.isEmpty()) {
            iconPath = path.local8Bit();
            if (!iconPath.isEmpty() && access(iconPath.data(), R_OK) == 0)
                iconFile = iconPath.data();
        }
    }

    nm_notifier_glue_show(summary.utf8().data(), body.utf8().data(), iconFile);
}

void NmNotifier::updateFromData(const NmData &data)
{
    TQString name = data.primaryConnectionName();
    TQString typeLabel = data.primaryConnectionTypeLabel();
    bool hasConnection = !name.isEmpty();

    if (!m_seeded) {
        m_seeded = true;
        m_hadConnection = hasConnection;
        m_lastConnectionName = name;
        m_lastConnectionTypeLabel = typeLabel;
        return;
    }

    if (m_hadConnection && hasConnection && name != m_lastConnectionName) {
        showNotification(
            i18n("Connection established"),
            i18n("Now connected to %1 '%2'.")
                .arg(typeLabel)
                .arg(name),
            data.primaryConnectionIconName());
    } else if (!m_hadConnection && hasConnection) {
        showNotification(
            i18n("Connection established"),
            i18n("Now connected to %1 '%2'.")
                .arg(typeLabel)
                .arg(name),
            data.primaryConnectionIconName());
    } else if (m_hadConnection && !hasConnection && !m_lastConnectionName.isEmpty()) {
        if (data.shouldNotifyConnectionLost()) {
            showNotification(
                i18n("Connection lost"),
                i18n("No longer connected to %1 '%2'.")
                    .arg(m_lastConnectionTypeLabel)
                    .arg(m_lastConnectionName),
                NmIcons::notConnectedIcon());
        }
    }

    m_hadConnection = hasConnection;
    if (hasConnection) {
        m_lastConnectionName = name;
        m_lastConnectionTypeLabel = typeLabel;
    }
}

#include "nmnotifier.moc"
