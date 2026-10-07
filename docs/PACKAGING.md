# Fedora packages

`packaging/build-rpm.sh` makes a source tarball from an explicit allowlist, then builds binary/source RPMs with Fedora's RPM macros. It dynamically links system Qt; Chromium/Qt are not bundled. Dependencies are generated from ELF linkage, with Wayland support explicitly required. The RPM contains only a binary, avatar icon in standard hicolor PNG sizes, Desktop Entry, AppStream metadata, docs, and license notices. There are no install/remove scripts that touch user state.

The source timestamp defaults to 2026-10-06 00:00:00 UTC and may be overridden with `SOURCE_DATE_EPOCH`. The build host is fixed to `reproducible.invalid`; RPM build timestamps/file mtimes use the source epoch. Builds use Fedora's compiler/hardening macros and run the synthetic tests under a private D-Bus session. Reproducibility is scoped to identical source, toolchain, architecture, and build configuration, not arbitrary future Fedora updates.

```sh
./packaging/build-rpm.sh
cp dist/qtclaw-aaha-0.1.0-0.1.beta.fc44.x86_64.rpm /tmp/qtclaw-first.rpm
./packaging/build-rpm.sh
sha256sum /tmp/qtclaw-first.rpm dist/qtclaw-aaha-0.1.0-0.1.beta.fc44.x86_64.rpm
```

For a clean-environment dependency/build check, use Fedora `mock` with the generated SRPM on a prepared Fedora 44 builder. This is a separate acceptance step; an on-host rpmbuild does not prove a clean chroot build. For publication, retain source tarball/SRPM/checksums with the release. Package signing, GitHub publication, and a COPR repository are separate distribution decisions.

`packaging/Fedora.Containerfile` also defines a Fedora 44 build environment for rootless Podman:

```sh
podman build --file packaging/Fedora.Containerfile --tag localhost/qtclaw-aaha-fedora44-check .
```

The container check copies an allowlisted source archive into this image, builds through `packaging/build-rpm.sh` as its non-root builder, and uses default container security with the Chromium sandbox enabled. A separate bare Fedora 44 container checks DNF runtime dependency resolution, installation, file verification, reinstall, and removal with synthetic user data. This does not exercise a KDE desktop or replace authenticated acceptance. Fedora repositories and the image tag can change; identical toolchain inputs remain necessary for a byte-for-byte reproducibility comparison.

A per-user development launcher may point directly at `build/dev/qtclaw-aaha` while the RPM is not installed. When switching to the packaged installation, remove or replace that development Desktop Entry under the user's applications directory so it does not shadow the package's launcher with the same application ID. This launcher does not contain Gateway credentials or alter browser profiles.
