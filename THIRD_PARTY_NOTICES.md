# Third-party notices

QtClaw-aaha's C++ code and compatibility adapter are MIT licensed. The application and tray badge was supplied by the project owner from their profile avatar and prepared for this project at their request; its provenance is recorded in assets/ICON.md. The source screenshot does not establish the original artwork's author or license. QtClaw-aaha is an independent wrapper and is not an official OpenClaw or Qt release.

The OpenClaw UI is loaded from the user's own Gateway. Its source/assets are not copied or shipped here. OpenClaw is Copyright 2026 OpenClaw Foundation, MIT licensed: https://github.com/openclaw/openclaw/blob/main/LICENSE. The notification adapter interoperates with the public wire contract in `ui/src/app/native-notifications.ts`; it does not copy the TypeScript implementation.

QtClaw-aaha dynamically links Fedora's Qt libraries. Qt WebEngine's Qt-specific components are available under LGPLv3/GPL/commercial terms; Chromium and its components have additional licenses. Fedora distributes those dependencies and their license notices separately. The wrapper's MIT license does not replace dependency licenses. Qt's QWebChannel JavaScript is obtained at runtime from Qt's resource library and executes only in the isolated world. See https://doc.qt.io/qt-6.11/qtwebengine-licensing.html and the Fedora Qt package license metadata. Distributors must preserve the applicable dependency notices and LGPL rights; this project does not bundle Qt or Chromium.

The freedesktop notification protocol is implemented through Qt D-Bus. Specification: https://specifications.freedesktop.org/notification/latest/protocol.html.
