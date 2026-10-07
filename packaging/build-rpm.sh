#!/usr/bin/env bash
set -euo pipefail
qtclaw_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
qtclaw_dist="$qtclaw_root/dist"
qtclaw_rpmroot="$qtclaw_dist/rpmbuild"
export SOURCE_DATE_EPOCH=${SOURCE_DATE_EPOCH:-1791244800}
if [[ ! "$SOURCE_DATE_EPOCH" =~ ^[0-9]+$ ]]; then
    printf '%s\n' 'SOURCE_DATE_EPOCH must be a Unix timestamp.' >&2
    exit 1
fi
mkdir -p "$qtclaw_rpmroot"/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS,TMP}
tar --sort=name --mtime="@$SOURCE_DATE_EPOCH" --owner=0 --group=0 --numeric-owner \
    --transform='s,^,qtclaw-aaha-0.1.0/,' -C "$qtclaw_root" -cf - \
    AGENTS.md .gitignore .containerignore CMakeLists.txt CMakePresets.json LICENSE THIRD_PARTY_NOTICES.md README.md \
    src assets tests docs packaging | gzip -n > "$qtclaw_rpmroot/SOURCES/qtclaw-aaha-0.1.0.tar.gz"
cp "$qtclaw_root/packaging/qtclaw-aaha.spec" "$qtclaw_rpmroot/SPECS/"
rpmbuild -ba --define "_topdir $qtclaw_rpmroot" --define "_tmppath $qtclaw_rpmroot/TMP" --define '_buildhost reproducible.invalid' \
    "$qtclaw_rpmroot/SPECS/qtclaw-aaha.spec"
cp "$qtclaw_rpmroot/SOURCES/qtclaw-aaha-0.1.0.tar.gz" "$qtclaw_dist/"
cp "$qtclaw_rpmroot"/RPMS/*/qtclaw-aaha-*.rpm "$qtclaw_dist/"
cp "$qtclaw_rpmroot"/SRPMS/qtclaw-aaha-*.src.rpm "$qtclaw_dist/"
(cd "$qtclaw_dist" && sha256sum qtclaw-aaha-0.1.0.tar.gz qtclaw-aaha-*.rpm > SHA256SUMS)
printf 'Packages and checksums: %s\n' "$qtclaw_dist"
