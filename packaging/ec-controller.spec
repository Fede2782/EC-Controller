%global debug_package %{nil}

Name:           ec-controller
Version:        1.0.2
Release:        1%{?dist}
Summary:        Control MSI laptop hardware through the msi-ec driver

License:        GPL-3.0-only
URL:            https://github.com/Fede2782/EC-Controller
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc-c++
BuildRequires:  make
BuildRequires:  cmake
BuildRequires:  extra-cmake-modules
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qttools-devel
BuildRequires:  kf6-kcoreaddons-devel
BuildRequires:  kf6-kglobalaccel-devel
BuildRequires:  kf6-knotifications-devel
BuildRequires:  polkit-qt6-1-devel
BuildRequires:  desktop-file-utils
BuildRequires:  systemd-rpm-macros

Requires:       polkit
Requires:       dbus
Recommends:     qt6-qtsvg
Recommends:     qt6-qtwayland
Recommends:     upower
Recommends:     libkscreen
%{?systemd_requires}

%description
A graphical control panel and background service for MSI laptops on Linux.
EC Controller configures fan curves, performance profiles, battery charging
thresholds, keyboard backlights, and hardware switches through the msi-ec
kernel driver.

%prep
%setup -q

%build
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build --parallel

%install
DESTDIR=%{buildroot} cmake --install build

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/org.fedelabs.EcController.desktop

%post
%systemd_post org.fedelabs.eccontroller.service

%preun
%systemd_preun org.fedelabs.eccontroller.service

%postun
%systemd_postun_with_restart org.fedelabs.eccontroller.service

%files
%license LICENSE
%doc README.md
%{_bindir}/ec-controller
%{_bindir}/ec-controller-service
%{_datadir}/dbus-1/system.d/org.fedelabs.EcController.conf
%{_datadir}/polkit-1/actions/org.fedelabs.EcController.policy
%{_datadir}/polkit-1/rules.d/49-org.fedelabs.EcController.rules
%{_unitdir}/org.fedelabs.eccontroller.service
%{_datadir}/applications/org.fedelabs.EcController.desktop
%{_datadir}/metainfo/org.fedelabs.EcController.metainfo.xml
%config(noreplace) %{_sysconfdir}/xdg/autostart/ec-controller-autostart.desktop
%{_datadir}/icons/hicolor/*/*/ec-controller*
%{_datadir}/icons/breeze/*/*/ec-controller*
%{_datadir}/icons/breeze-dark/*/*/ec-controller*
%{_datadir}/pixmaps/ec-controller*

%changelog
* Thu Oct 24 2026 Fede2782 <78815152+Fede2782@users.noreply.github.com> - 1.0.2-1
- Improve strings formatting
- Add About Qt menu
- Italian localization
- Pause unnecessary operations when minimized

* Thu Oct 22 2026 Fede2782 <78815152+Fede2782@users.noreply.github.com> - 1.0.1-1
- Reinitialize GUI and hardware monitors on reload
- Fix window height expansion in single-box edit mode
- Rename desktop launcher to org.fedelabs.EcController.desktop and update AppStream categories

* Tue Oct 13 2026 Fede2782 <78815152+Fede2782@users.noreply.github.com> - 1.0.0-1
- Initial release
