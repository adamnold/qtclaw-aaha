# Architecture and security boundaries

`Config` stores schema-versioned, atomically written server configuration. Immutable UUIDs name profile directories. `Policy` validates secure endpoints, base paths, authentication origins, and the notification wire contract. Credentials and URL tokens are not accepted in configuration.

`MainWindow` creates `ServerSession` lazily. Each session owns a named persistent `QWebEngineProfile` and its main page/view. Auth windows share only their originating profile and have no QWebChannel. All views/pages are destroyed before the profile so the engine can flush state. Cookie lifetimes follow Qt's `AllowPersistentCookies`; permissions use `StoreOnDisk`. Cache size is capped per profile. Qt's default desktop User-Agent and upstream CSP remain intact.

`SingleInstance` uses an account-private runtime directory, `QLockFile`, and a user-access-only local socket. Only a bounded activation command is accepted. A live profile lock is never expired on a timer. A dead-owner lock is recoverable through Qt's stale-process checks.

## Notification transport

Scripts at document creation run only on the configured Gateway's top-level base-path page. The main-world shim implements OpenClaw's existing `window.webkit.messageHandlers.openclawNotifications.postMessage` interface, snapshot, and status event. A separate ApplicationWorld script holds `qt.webChannelTransport`. Main-world code cannot access that transport or arbitrary Qt objects.

The two worlds exchange bounded strings through `window.postMessage`. The isolated listener verifies source window and origin. The C++ bridge additionally checks current page origin/base path, exact message type/fields, size/rate, run identifier, and relative session route. Same-origin code that controls the trusted top-level Gateway can invoke the notification API by design; the bridge is not a defense against a compromised Gateway's own code. It still exposes no filesystem, credential, shell, or agent capability.

Native completion routes accept only an in-base-path absolute path and an optional `session` query. Transport metadata is never displayed as notification content or written to logs. A bounded per-session run-id set suppresses duplicates while the session lives; it is not persisted across application restarts. Failed deliveries can be retried. D-Bus delivery is asynchronous and reports test errors; a daemon's acceptance does not guarantee visibility under KDE's notification policy.

Upstream contract: https://github.com/openclaw/openclaw/blob/main/ui/src/app/native-notifications.ts. OpenClaw notification semantics: https://docs.openclaw.ai/web/notifications. This is a compatibility interface without a version handshake; future changes may need an adapter update. The normal web UI remains available independently.

## Browser policy

TLS errors are always rejected. Production URLs require HTTPS; loopback HTTP allows local fixtures/development. Main-frame navigation remains on the Gateway origin, approved auth windows, or user-activated ordinary external links. File/qrc main-frame navigation is blocked. The normal upstream resource graph/CSP is not replaced by a third-party hostname allowlist. Qt Web Push is disabled and browser notifications are denied; native completion notifications are separate.

Microphone, camera, and clipboard requests show explicit origin-aware prompts. Unsupported features, browser notifications, and screen capture are denied. Native Qt upload/save dialogs and download requests handle user-selected files; File System Access requests have an additional confirmation. F11 provides wrapper full screen while site full-screen requests are declined.

`Diagnostics` rotates at approximately 64 KiB, retains one previous file, and records fixed codes, validated UUIDs, and sanitized origins. Web console output is suppressed to avoid logging conversation data. Qt/Chromium's own system diagnostics are not included in the wrapper's export and may differ by Fedora build.

The app rejects externally supplied sandbox/TLS/web-security/remote-debugging overrides. It does not disable SELinux or globally modify Fedora trust/configuration. A user with account-level access can still inspect browser files; private permissions do not imply disk encryption.
