# QtClaw-aaha

A small C++/Qt desktop wrapper for your existing OpenClaw Gateway. Initial beta target: Fedora 44 KDE, x86-64, Wayland. QtClaw-aaha uses Fedora's shared Qt WebEngine and leaves the live OpenClaw interface intact.

## What it does

- Multiple named servers with separate persistent browser profiles, cookies, storage, and site permissions.
- Native navigation, zoom, uploads, download progress/cancellation, and permission prompts.
- Close to tray, explicit Quit, single-instance activation, and optional login autostart.
- Generic KDE notifications for OpenClaw's background-session completion contract, with click routing to the originating server/session.
- TLS verification and Chromium sandboxing, narrow isolated-world notification bridge, bounded diagnostics.

This beta is implemented and has synthetic tests. Initial user testing confirmed sign-in, pairing, chat, tray behavior, restart persistence, and OpenClaw's background-session flow. The remaining desktop acceptance is recorded in [validation](docs/VALIDATION.md) and the [acceptance checklist](docs/ACCEPTANCE.md).

## Build and run

```sh
sudo dnf install gcc-c++ cmake ninja-build qt6-qtbase-devel qt6-qtwebengine-devel qt6-qtwebchannel-devel dbus-daemon openssl
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
./build/dev/qtclaw-aaha
```

Add a server through **Settings → New server**. Enter its HTTPS Gateway base address (including any deployment path), then sign in and approve device pairing inside OpenClaw as usual. HTTP is accepted only for loopback development servers. Wrapper settings reject token-bearing URLs; credentials stay in the Gateway's browser storage.

Qt 6.10 or newer is required so storage paths are fixed before profile creation. The initial build uses Fedora's installed Qt 6.11.2.

Add authentication origins only if your sign-in flow needs them. Such windows share that server's profile, show their origin, and have no notification bridge. Ordinary external links open in your default browser. An existing server's base address is immutable; create another server to change it.

**Closing the window keeps opened dashboards running.** Quit exits. If no system tray is available, Close minimizes the window so you can restore it. Saved servers connect when first opened; unopened servers do not consume a renderer.

## Notifications and browser state

Allow native notifications in server settings or when OpenClaw requests them. OpenClaw's **Settings → Notifications → Send test** sends a sample desktop alert. To start background work in the tested OpenClaw v2026.9.4 interface, click **+** beside **SESSIONS**, enter the first message, and press **Ctrl+Enter**. If Ctrl+Enter is configured for ordinary sending, use **Ctrl+Shift+Enter**. Close QtClaw-aaha to the tray before completion to check the desktop alert; clicking it should restore that chat. OpenClaw's alert inside its page is a separate notification.

QtClaw-aaha supports `status`, `request-permission`, `send-test`, and `background-session-completed`. It does not spoof another native platform or expose browser management, shell, file, or native-agent commands.

Completion alerts use `org.freedesktop.Notifications`, not Google-backed Web Push. They require the originating dashboard to remain loaded. They do not include the full web-push set of approvals, mentions, questions, or automation events, and cannot arrive after Quit or during sleep. This transport choice does not remove other third-party resources that your Gateway's UI itself uses.

Persistent profiles preserve OpenClaw's browser storage and cryptographic device keys. QtClaw-aaha respects upstream session-cookie/password lifetimes rather than forcing them into persistent wrapper credentials. Profiles are private to your Unix account, but are not promised to be encrypted at rest. Secure your account and disk accordingly.

## RPM and source distribution

```sh
sudo dnf install rpm-build redhat-rpm-config desktop-file-utils appstream dbus-daemon
./packaging/build-rpm.sh
sudo dnf install ./dist/qtclaw-aaha-0.1.0-0.1.beta.fc44.x86_64.rpm
```

The script produces an RPM, SRPM, source tarball, and checksums under `dist/`. It uses a fixed source timestamp and excludes Git state, build products, and browser profiles. Rebuilding the same source with the same Fedora toolchain can be checked using the instructions in [packaging](docs/PACKAGING.md). Engine/security updates come through Fedora's Qt packages; no bundled engine updater is included.

Removal: `sudo dnf remove qtclaw-aaha`. Ordinary package removal and upgrades preserve your saved settings and browser profiles. Disable login autostart before removal if enabled. **Reset local data** and **Remove server** explicitly confirm deletion of that server's local state. They do not alter server-side conversations.

Default per-user directories (KDE/XDG overrides are respected):

| Purpose | Default path |
| --- | --- |
| Settings | `~/.config/Adam And His Agents/qtclaw-aaha/settings.json` |
| Profiles and sanitized logs | `~/.local/share/Adam And His Agents/qtclaw-aaha/` |
| Cache | `~/.cache/Adam And His Agents/qtclaw-aaha/` |
| Instance lock/socket | `$XDG_RUNTIME_DIR/qtclaw-aaha/` |
| Opt-in autostart | `~/.config/autostart/ch.adamsagents.qtclaw.desktop` |

No server is preconfigured, and no endpoint, account data, or browser profile is shipped. QtClaw-aaha does not install OpenClaw or change Gateway configuration. Qt WebEngine remains a Chromium-based renderer; performance claims require measurement.

MIT wrapper. The application and tray badge use the user-supplied AAHA avatar; see [asset provenance](assets/ICON.md). OpenClaw and dependency attribution: [third-party notices](THIRD_PARTY_NOTICES.md).
