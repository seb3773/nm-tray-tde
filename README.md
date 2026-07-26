# nm-tray-tde

NetworkManager system tray applet for [Trinity Desktop Environment](https://www.trinitydesktop.org/) (TQt3/TDE).

> See [LOGIC.md](LOGIC.md) for detailed diagrams of the internal logic, connecting animation, and Wi-Fi scan strategy.

## Why nm-tray-tde?

**TDEnetworkmanager** (Trinity’s legacy network manager) relied on **tdehwlib** / `TDEGlobalNetworkManager` — a TDE-specific abstraction on top of NetworkManager. That stack has aged a bit and is less aligned with NetworkManager’s current API.

**nm-tray-tde** takes a different approach:

- **Business logic** from [network-manager-applet](https://gitlab.gnome.org/GNOME/network-manager-applet) (nm-applet): `libnm`, NM signals, Wi-Fi scan, secret agent, signal-strength icon tiers, and related behaviour.

The natively implemented **Wi-Fi hotspot** (“Create New Wi-Fi Network”) is a genuine differentiator: TDEnetworkmanager did not offer hotspot sharing directly from the system tray menu.

## Comparison with other applets

- **TDEnetworkmanager (legacy)**: Relies on an aging abstraction (`tdehwlib`). Reads NM state passively without explicit scan policies, causing intermittent staleness on modern hardware. Lacks advanced features like hotspot creation.
- **GNOME nm-applet**: The gold standard, but heavily tied to GTK/GNOME. Forces a scan on menu open, which guarantees freshness but can cause noticeable UI delays.
- **nm-tray (Qt)**: Opens instantly, but no background scan policy means the network list degrades over time.
- **nm-tray-tde (this project)**: Uses direct `libnm` calls for robust business logic while integrating natively with TQt3/TDE. Features a hybrid scan strategy (instant open + on-demand freshness) and advanced features like **Wi-Fi hotspot creation**. Runs as a normal session user.

## Notable points

_This section will grow over time._

- **Modern NM API** — `libnm` (`NMClient`, `NMDevice`, `NMAccessPoint`, …), not deprecated `libnm-glib`.
- **No Qt model/view port** — flat `NmData` + `NmItem` lists instead of porting `QAbstractItemModel` to TQt3.
- **GLib ↔ TQt3** — `NmEventPump` (`TQTimer` + `g_main_context_iteration()`), no D-Bus fd hacks.
- **Secret agent** — `NMSecretAgentOld` + `TQInputDialog` for WPA-PSK (session user, not root).
- **Menu model** — left click: connection menu; right click: quick actions (enable/disable networking, Wi-Fi, quit).
- **Wi-Fi UX** — in-range networks in a flat list; out-of-range saved profiles under **Saved networks**; connected SSID excluded from “other” entries.
- **libnotify** — network connection established / lost / changed notifications (`NmNotifier`, configured via `nmtrayrc`); "established" triggered only in `ACTIVATED` state.
- **VPN tray icon** — `nm_vpn_active` icon shown when a VPN is active (matches nm-applet logic).
- **Native dialogs only** — no `fork`/`exec`/`nm-connection-editor`; all UI in TQt3 via libnm.
- **Hidden Wi-Fi dialog** — connect to non-broadcast SSID.
- **Connection editor** — list, delete, create and edit Wi-Fi, wired, and VPN profiles.
- **Connection information** — active connection details (IP, DNS, Wi-Fi signal, speed, …); greyed out when no connection is active.
- **Connecting animation** — `nm_connect_stage0`–`6` icons during activation (yabatman pattern).
- **Wi-Fi hotspot** — "Create New Wi-Fi Network" is natively implemented in the tray menu.

## Wi-Fi scan strategy

NetworkManager exposes Wi-Fi access points through `nm_device_wifi_get_access_points()`. That list is only reliable when a recent scan has completed.

### How nm-tray-tde handles it

1. **Instant open** — Show the menu immediately from a **persistent SSID cache** (`m_wifiNetworkCache`).
2. **On-demand freshness** — To avoid UI freezes and scan spam, an automatic background scan is only triggered if the cache is virtually empty (≤ 1 AP). Otherwise, the user manually requests a scan via the "Wifi - request scan" menu option.
3. **Non-blocking wait** — When a scan is running, poll the GLib main loop every 200 ms. Retries `RequestScan` if rejected due to driver rate-limiting, and refreshes the menu as new APs arrive.
4. **Stable cache** — Prune entries only after **5 consecutive misses** when a reasonably full scan snapshot is available (`apCount > 2`), preventing partial snapshots from wiping the list.

## Stack (overview)

| Layer | Technology |
|-------|------------|
| UI | TQt3, `KSystemTray`, `TDEPopupMenu` |
| NM | libnm, `NmClient`, `NmSecretAgentOld` |
| Event loop | `NmEventPump` |
| Build | CMake, TDE macros |

See [LOGIC.md](LOGIC.md) for sequence and state diagrams of the applet's logic.

## References

- [network-manager-applet](https://gitlab.gnome.org/GNOME/network-manager-applet) — NM business logic
- [TDEnetworkmanager](https://github.com/trinitydesktop/tdenetworkmanager) — TDE systray and UI patterns

## Build (draft)

Requires a Trinity Desktop session (`/opt/trinity` or `TDE_PREFIX`), `libnm-dev`, and a C++ toolchain.

```bash
./build.sh
./build/src/nm-tray-tde
```

The applet is built as a **single executable** (no `libtdeinit_*.so` kdeinit split).

