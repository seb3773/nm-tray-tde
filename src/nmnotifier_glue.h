#ifndef NM_NOTIFIER_GLUE_H
#define NM_NOTIFIER_GLUE_H

#ifdef __cplusplus
extern "C" {
#endif

int nm_notifier_glue_init(const char *app_name);
void nm_notifier_glue_shutdown(void);
void nm_notifier_glue_show(const char *summary, const char *body, const char *icon_path);

#ifdef __cplusplus
}
#endif

#endif
