%global debug_package %{nil}
%global use_source_date_epoch_as_buildtime 1
%global clamp_mtime_to_source_date_epoch 1
Name:           qtclaw-aaha
Version:        0.1.0
Release:        0.1.beta%{?dist}
Summary:        Fedora desktop wrapper for OpenClaw Gateways
License:        MIT
URL:            https://github.com/adamnold/qtclaw-aaha
Source0:        %{name}-%{version}.tar.gz
BuildRequires:  gcc-c++
BuildRequires:  cmake
BuildRequires:  ninja-build
BuildRequires:  qt6-qtbase-devel >= 6.10
BuildRequires:  qt6-qtwebengine-devel >= 6.10
BuildRequires:  qt6-qtwebchannel-devel >= 6.10
BuildRequires:  openssl
BuildRequires:  dbus-daemon
BuildRequires:  desktop-file-utils
Requires:       qt6-qtwayland

%description
QtClaw-aaha wraps an existing OpenClaw Gateway UI with Qt Widgets and
Fedora's shared Qt WebEngine. It includes isolated persistent server
profiles, tray behavior, downloads and native completion notifications.

%prep
%autosetup

%build
%cmake -GNinja -DBUILD_TESTING=ON
%cmake_build

%check
ctest --test-dir %{_vpath_builddir} --output-on-failure
desktop-file-validate packaging/ch.adamsagents.qtclaw.desktop

%install
%cmake_install

%files
%{_bindir}/qtclaw-aaha
%{_datadir}/applications/ch.adamsagents.qtclaw.desktop
%{_datadir}/icons/hicolor/*/apps/ch.adamsagents.qtclaw.png
%{_datadir}/metainfo/ch.adamsagents.qtclaw.metainfo.xml
%license %{_datadir}/licenses/qtclaw-aaha/
%doc %{_datadir}/doc/QtClawAAHA/

%changelog
* Tue Oct 06 2026 Adam And His Agents - 0.1.0-0.1.beta
- Initial independent Qt/Fedora wrapper beta.
