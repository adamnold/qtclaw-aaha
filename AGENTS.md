# QtClaw-aaha

This folder is the independent project root. The parent AAHA portfolio instructions and operations documents are obsolete for this project; do not apply them or walk up to them.

QtClaw-aaha wraps an existing OpenClaw Gateway UI with C++/Qt Widgets and Fedora's shared Qt WebEngine. It does not fork the frontend, install OpenClaw, or expose native agent capabilities. The wrapper is MIT; dependencies retain their licenses.

Never commit private endpoints, tokens, passwords, browser state, or user conversations. Test with loopback fixtures and disposable profiles. Do not weaken TLS or disable the Chromium sandbox. QWebChannel must stay in ApplicationWorld and expose only the notification contract on the configured top-level Gateway page.

Build: `cmake --preset dev && cmake --build --preset dev`. Tests: `ctest --preset dev` (the test runner creates a private D-Bus/XDG environment). Package: `./packaging/build-rpm.sh`. Keep authenticated and KDE user acceptance distinct from automated test evidence. Record limitations honestly in docs/VALIDATION.md.
