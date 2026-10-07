# 0.1.0 beta acceptance

Run these interactively on Fedora 44 KDE Wayland, alongside the existing app. Use disposable servers/profiles for destructive or failure tests. Do not disable TLS verification, pairing, or the Chromium sandbox to make a test pass. Automated test results are recorded separately in VALIDATION.md.

## Sign-in and isolation

- Add two servers, including a non-root base-path deployment. Verify normal sign-in, device approval, streaming chat, and navigation.
- Switch servers: chats, device keys, cookies, tokens, and permissions must remain isolated. Opened servers stay running while hidden; unopened servers stay disconnected.
- Quit and relaunch; confirm each server's durable pairing/storage survives. Session-only credentials may require re-entry by upstream design.
- Install an upgrade and remove/reinstall the package; verify saved data survives. Cancel each reset/remove confirmation and verify nothing is deleted. Confirmed reset affects only the chosen local profile.
- Launch a second instance while visible/hidden; it activates the existing process and does not create a second profile writer.

## Desktop and notifications

- Close to tray, restore, explicit Quit, and opt-in login autostart. Test the no-tray fallback.
- Allow, deny, and test native notifications using OpenClaw Settings → Notifications → Send test. In the tested v2026.9.4 UI, click + beside SESSIONS, enter a first message, and press Ctrl+Enter (Ctrl+Shift+Enter if Ctrl+Enter is configured for ordinary sending). Close QtClaw-aaha to the tray before completion and verify a KDE desktop alert. Clicking it restores the correct server and session. Record OpenClaw's in-page alert separately. Duplicate completion events should not create duplicate desktop notices.
- Check KDE Do Not Disturb behavior. Approval/mention/automation web-push alerts are outside this beta's native contract.

## Files and media

- Native upload dialog, drag/drop attachment, multiple uploads, download save dialog, progress, cancellation, collision/overwrite confirmation, and interrupted download.
- Normal copy/paste and browser clipboard permission prompts. Grant/deny microphone/camera requests; verify recording, playback, and device selection in OpenClaw. Reset site permissions and verify requests are asked again.
- Normal external links use the default browser. Approved authentication origins open identified profile-sharing windows; unapproved redirects/schemes stay blocked. Authentication callbacks return to the correct server.

## Recovery and security

- During an active session, disconnect/reconnect Tailscale, restart the Gateway, and suspend/resume the laptop. Existing loaded pages keep their state and use upstream reconnect behavior. No wrapper reload loop should occur.
- Terminate a disposable renderer and recover with Reload; sign-in/storage must remain intact.
- Invalid/expired/untrusted TLS certificates must be rejected without an override.
- Repeat desktop acceptance on an SELinux-enforcing Fedora installation. The initial development host was observed in permissive mode; no policy was changed by QtClaw-aaha's implementation work.
- Verify bridge absence on auth windows, subframes, and out-of-base-path pages. Malformed/oversized notifications and cross-origin/session routes must be rejected.
- Inspect exported diagnostics: codes, server identifiers, and origins only; no chat text, URL path/query/fragment, password, or token. Review hostnames before sharing.

## Performance record

Record the exact app/Qt/OpenClaw versions, server count, workflow, CPU/RAM/GPU, idle RSS/PSS and CPU, load-to-interactive time, and comparable streaming/attachment latency. Measure the same workload in the existing app before making any comparison. Do not call headless test timing a desktop responsiveness benchmark.
