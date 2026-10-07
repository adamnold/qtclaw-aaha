# Application icon provenance

The project owner supplied a screenshot of their profile avatar on 2026-10-06 and requested its use for QtClaw-aaha's application and tray icons. The source screenshot is not shipped. Its original artwork author and license were not independently established.

The built-in imagegen tool prepared a transparent circular badge using that screenshot as the edit target. This is a generated cleanup of the supplied avatar; fine illustration details can differ from the screenshot. `icon-master.png` is the selected 1254×1254 RGBA output. Qt's smooth image scaling produces the PNG resolution variants under `icons/`; each keeps its transparent background. The compiled app embeds the variants, and CMake/RPM installs them under the standard hicolor icon directories. No theme icon is substituted for the tray badge.

Final imagegen prompt:

```text
Use case: background-extraction
Asset type: Fedora/KDE application and system-tray icon.
Input image: edit target, the user's supplied profile-avatar screenshot. Extract its existing circular badge as faithfully as possible. Remove only the surrounding white screenshot background and margins, replacing them with true transparency. Preserve the red lobster illustration, pose, claws, grey highlights, black circular disc, red glowing inner ring, and salmon thin outer border unchanged. Crop tightly around the entire circle, center it on a square transparent canvas with just a tiny transparent safety margin, and provide a clean high-resolution icon master. No redesign, no added elements, no text, no white square, no shadows outside the circle.
```
