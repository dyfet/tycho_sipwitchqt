Name: sipwitchqt
Epoch:   1
Version: 0.1.0
Release: 1
Summary: Sip server system daemon
License: GPLv3+
URL:     https://gitlab.com/tychosoft/sipwitchqt
BuildRequires: qt5-qtbase-devel, gcc-c++
BuildRequires: libosip2-devel, libeXosip2-devel >= 4.0.0
BuildRequires: gperftools-devel, systemd-devel
Requires: qt5-qtbase-mysql

%description
A pbx server for the sip protocol

%prep
%setup -q

%build
qmake-qt5 CONFIG+=sys_prefix QMAKE_CXXFLAGS+="\"%optflags\"" QMAKE_STRIP="/bin/true"
%{__make} %{?_smp_mflags}

%install
%{__make} install INSTALL_ROOT=%{buildroot}

%files
%defattr(-,root,root)
%doc README.md DOCKER.md CONTRIBUTING.md LICENSE CHANGELOG
%{_sbindir}/sipwitchqt
%{_sbindir}/sipwitchqt-daemon
%{_sysconfdir}/sv/sipwitchqt/run
%attr(0664,root,root) %config(noreplace) %{_sysconfdir}/sipwitch.conf



