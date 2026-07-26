# nm-tray-tde Architecture

Native NetworkManager tray applet for Trinity Desktop (TQt3/TDE).

## Stack

| Layer | Technology |
|-------|------------|
| UI | TQt3, KSystemTray, TDEPopupMenu |
| NM backend | libnm (`NMClient`, `NMSecretAgentOld`) |
| GLib pump | `NmEventPump` (TQTimer → `g_main_context_iteration`) |
| Notifications | libnotify (pattern yabatman) |
| Build | CMake + TDE macros |

## Class diagram

```
TrayApp (KUniqueApplication)
  └── TrayController
        ├── NmEventPump
        ├── NmClient
        ├── NmData (+ wifiutil helpers)
        ├── NmNotifier
        ├── NmTray (KSystemTray)
        ├── NmTrayPopup (main + quick + saved submenu)
        └── Dialogues natifs (hidden Wi-Fi, éditeur, infos connexion)
```

## References

- Business logic: `network-manager-applet` (nm-applet)
- TDE patterns: `tdenetworkmanager`
- Notifications: `yabatman` (libnotify)

## Phases

1. **Done**: CMake, NmClient, NmEventPump, KSystemTray, nm-probe, NmSecretAgent, NmData, menus connexions, dialogues natifs, notifications, animation connecting
2. **En cours**: polish UX menus (sous-menu Saved networks, scan hybride), factorisation `wifiutil`
3. **Finition**: icônes `pics/`, autostart, packaging `BUILD_TOOLS=OFF`, i18n interne (toute fin)
4. **Phase finale** (différenciateur vs TDEnetworkmanager legacy) :
   - **Hotspot Wi-Fi** (« Create New Wi-Fi Network ») — dialogue natif mode AP

### Dialogues natifs (remplace nm-connection-editor)

| Dialogue | État |
|----------|------|
| Hidden Wi-Fi | Fait — SSID, sécurité, mot de passe, `NM_SETTING_WIRELESS_HIDDEN` |
| Edit Connections (liste + supprimer) | Fait |
| Édition profil Wi-Fi / câblé / VPN | Fait |
| Infos connexion | Fait — grisé sans connexion active |
| Animation « connecting » | Fait — icônes stage0–6, tray + menu |
| Hotspot | Phase finale |

Références UI : `tdenetworkmanager` (TQt3, `.ui`), logique NM : `nm-applet` / libnma (sans GTK).

## Modules Wi-Fi / NM

- **`wifiutil`** — SSID, device primaire, filtrage AP → profil sauvegardé (`g_ptr_array_unref` correct).
- **`NmEventPump`** — `pump()`, `pumpAfterAsync()`, `pumpRepeated()` après appels NM async.
- **Déconnexion VPN** — `deactivateActiveConnection(uuid|path)` avant fallback device Wi-Fi.
- **Notifications** — seed complet; « established » seulement si connexion primaire `ACTIVATED`; changement A→B : une seule notif « established » (pas de « lost » pendant la transition NM).
- **Menu scan refresh** — pas de `rebuild()` tant que le sous-menu Saved networks est ouvert.

## Positionnement vs TDEnetworkmanager

| TDEnetworkmanager (legacy) | nm-tray-tde |
|--------------------------|-------------|
| `tdehwlib` / `TDEGlobalNetworkManager` | **libnm** (`NMClient`) — API NM actuelle |
| Éditeur connexions monolithique TDE | Éditeur natif TQt3 intégré (Wi-Fi / câblé / VPN) |
| Pas de hotspot systray | Hotspot nm-applet (phase finale) |
| Processus utilisateur, D-Bus NM | Idem — **pas de root** |
