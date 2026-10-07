FROM registry.fedoraproject.org/fedora:44

RUN dnf -y --setopt=install_weak_deps=False install \
      gcc-c++ cmake ninja-build qt6-qtbase-devel qt6-qtwebengine-devel \
      qt6-qtwebchannel-devel qt6-qtwayland openssl dbus-daemon \
      desktop-file-utils appstream rpm-build redhat-rpm-config \
      shadow-utils tar gzip findutils \
    && dnf clean all \
    && useradd --create-home --uid 1000 builder

USER builder
WORKDIR /home/builder
