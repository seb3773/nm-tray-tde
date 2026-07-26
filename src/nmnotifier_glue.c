#include "nmnotifier_glue.h"

#include <libnotify/notify.h>

int nm_notifier_glue_init(const char *app_name)
{
    return notify_init(app_name) ? 1 : 0;
}

void nm_notifier_glue_shutdown(void)
{
    notify_uninit();
}

void nm_notifier_glue_show(const char *summary, const char *body, const char *icon_path)
{
    NotifyNotification *notification;

    if (!summary || !body)
        return;

    notification = notify_notification_new(summary, body, icon_path);
    if (!notification)
        return;

    notify_notification_set_timeout(notification, 5000);
    notify_notification_show(notification, NULL);
    g_object_unref(notification);
}
