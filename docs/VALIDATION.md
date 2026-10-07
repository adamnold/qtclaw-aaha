# Validation status

Checked 2026-10-06 on Fedora 44 x86-64 with Qt 6.11.2, GCC 16.2.1, CMake 4.3.0, and RPM 6.0.2.

## Passed

- Debug CMake/Ninja build, with shared system Qt libraries.
- Twelve automated feature checks in one CTest executable (QtTest reports 14 passes including setup/cleanup): endpoint policy, notification payload policy, configuration/diagnostic privacy, single-instance activation, D-Bus notification delivery/click routing, browser bridge/navigation/downloads, completion contract/deduplication, self-signed certificate rejection, renderer failure/reload recovery, process-restart profile persistence/isolation, window/settings smoke behavior, and notification permission-dialog reentrancy.
- A repeated validated permission request during an open consent dialog reproduced overlapping native dialogs in the unguarded implementation. A per-server pending-dialog guard fixes it; the regression and full suite pass. This test exercises the native validated entry point directly during the modal event loop; a browser message burst did not reproduce the issue.
- Restart persistence uses separate child engine processes. Local storage, IndexedDB-held Ed25519 CryptoKeys/signatures, persistent cookies, and service-worker registration survive restart; a second profile on the same origin does not inherit state. The notification adapter is also checked on base paths containing encoded spaces and non-ASCII text.
- The application-loaded Gateway smoke probe succeeded in a new disposable profile: secure context, WebCrypto, IndexedDB, service-worker API, OpenClaw custom element, notification adapter; raw QWebChannel transport absent from MainWorld. The endpoint is not stored in source or packages. No credentials were supplied and no authenticated workflow was exercised.
- Fedora hardened RPM/SRPM build and packaged synthetic tests. Package manifest contains binary, desktop metadata, icon, docs, and licenses; Qt/Chromium and browser profiles are not bundled. No install/remove scriptlets touch user data.
- Account-local RPM relocation/install/remove experiment (isolated RPM database, dependencies supplied by the host): executable starts with the correct beta version; unrelated synthetic user data survives removal. This is a payload/lifecycle check, not a privileged DNF or clean-chroot test. Ownership checks are inapplicable to a non-root relocation.
- A separate rootless Fedora 44 container built the allowlisted source and RPM/SRPM using repository-installed build dependencies. Packaged tests passed as a non-root user with the Chromium sandbox enabled and the container's default security settings. A second, fresh Fedora 44 container resolved runtime dependencies through DNF, installed the RPM, passed RPM file verification and the version check, reinstalled it, and removed it. Synthetic settings/profile files survived reinstall and removal unchanged. This validates clean container build/dependency/lifecycle behavior; it is not a KDE, mock-chroot, SELinux-enforcing, or real browser-state upgrade test.
- Offscreen welcome/settings screenshots rendered and visually inspected for layout.

The application icon was replaced with a transparent cleanup of the owner's supplied profile avatar on 2026-10-06. The same embedded PNG variants supply the tray and native-window icons; desktop packaging installs 16–512 px hicolor variants. Resolution variants were visually inspected on light and dark backgrounds, the development build and existing automated suite passed, and the updated RPM/SRPM build passed its packaged tests. Their asset provenance is recorded in assets/ICON.md. A per-user development app-menu launcher and hicolor icons were installed for local acceptance. A subsequent user screenshot confirms the replacement native-window icon and the requested QtClaw-aaha title on KDE.

## User-reported KDE acceptance — 2026-10-06

The displayed application name was changed to `QtClaw-aaha` at the user's request. The internal application/storage ID, executable name, Desktop Entry ID, and profile directories keep their existing identifiers; changing the display name does not select a new storage location.

The user tested the current development build on their Gateway and reported:

- Token authentication accepted and device pairing approved.
- Chat works.
- Closing the window hides it to the tray.
- Quitting from the tray and reopening preserves sign-in/pairing state.
- A supplied screenshot shows the expected per-server native completion-notification permission dialog. This establishes that the permission request reached the native shell; it does not establish desktop notification delivery or click routing.
- Starting a new session in the background using Ctrl+Enter works. OpenClaw displays its completion alert inside the page, and opening the chat from that flow works.
- Fedora displays the desktop notification when a background session completes with the app hidden to the tray. Clicking that notification restores QtClaw-aaha and opens the relevant chat.

A narrowly scoped local diagnostic check found notification permission granted and two successful notification-service replies, with no bridge-rejection events in the inspected log. This confirms desktop D-Bus acceptance of two notification requests, not visible KDE banners or desktop click routing. The log does not identify which notification kinds those requests represented; private origins and identifiers are not included here.

These are user-reported results, not automated observations. They establish the above workflows on one server. Streaming details, two-server desktop isolation, and the remaining checklist are not inferred from them. No token, endpoint, device key, conversation, or screenshot is included in the public acceptance record.

## Limits and remaining acceptance

- Streaming details, real uploads/downloads, clipboard/microphone/camera, opt-in autostart, Tailscale outages, Gateway restarts, and suspend/resume still require the interactive checklist in ACCEPTANCE.md. Basic authentication, pairing, chat, close-to-tray, quit/relaunch persistence, OpenClaw's in-page background-session flow, and native desktop completion notification delivery/click routing now have the user-reported evidence above.
- A noninteractive PackageKit system-install attempt failed to obtain authentication; the host RPM was not installed. A system-wide DNF installation requires the user's sudo authentication. The independent clean Fedora container checks passed as described above; a Fedora mock-chroot build has not been performed.
- Headless/sandbox testing produced expected offscreen Vulkan/DRM access warnings and an NSS user-database initialization warning. No sandbox or TLS bypass was used. This does not validate GPU acceleration or interactive desktop performance.
- Direct host inspection reported SELinux **Permissive**. Sandbox inspection reported Disabled, which is not the host mode. Protected AVC audit access required sudo authentication and was unavailable; no claim is made that the host had no AVC events. No SELinux policy/mode was changed.
- Desktop Entry validation passed. AppStream validation reported a missing-homepage warning because no public project URL exists yet; it reported no metadata errors. Metadata deliberately does not invent a repository URL.
- No performance comparison, full release acceptance, package signing, or GitHub publication has been completed. Authenticated KDE and native notification acceptance are recorded above. The independent local repository and MIT source are ready for review.

Earlier source snapshots produced byte-identical RPM, SRPM, and source tarball hashes in consecutive builds with the installed Fedora toolchain, including the encoded-base-path fix and avatar icons. SHA256SUMS in dist identifies the current packaged artifacts; reproducibility comparisons apply only to the exact source snapshot and installed toolchain used for each comparison. No cross-toolchain reproducibility claim is made.
