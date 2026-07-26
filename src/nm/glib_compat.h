#ifndef NM_TRAY_GLIB_COMPAT_H
#define NM_TRAY_GLIB_COMPAT_H

/* TQt3 defines the macro "signals" which conflicts with GObject headers. */
#ifdef signals
#undef signals
#endif

#include <glib.h>
#include <NetworkManager.h>

#define signals protected

#endif
