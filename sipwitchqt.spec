Name: sipwitchqt
Epoch:   1
Version: 0.0.1
Release: 1
Summary: Sip server system daemon

License: GPLv3+
URL:     https://gitlab.com/tychosoft/sipwitchqt
Source0: https:///pub.cherokeesofidaho.org/tarballs/%{name}-%{version}.tar.gz
Group:	 system/telephony

BuildRequires: cmake >= 3.1.0
BuildRequires: qt5-qtbase-devel, gcc-c++
BuildRequires: libosip2-devel, libeXosip2-devel >= 4.0.0
BuildRequires: gperftools-devel, systemd-devel

%package mysql
Requires: sipwitchqt%{?_isa} = %{version}-%{release}
Requires: qt5-qtbase-mysql
Summary: sipwitchqt mysql support

%package sqlite3
Requires: sipwitchqt%{?_isa} = %{version}-%{release}
Requires: ruby-sqlite3
Summary: sipwitchqt sqlite3 local administration support

%package utils
Recommends: sipwitchqt
Requires: ruby
Summary: sipwitchqt utility scripts and commands

%package desktop
Summary: sipwitchqt desktop client 

%description
A pbx server for the sip protocol

%description utils
Extra utility commands and scripts to use with sipwitchqt.  Some may be used stand-alone.

%description mysql
Mysql backend support for SipWitchQt.

%description sqlite3
Sqlite3 local db administration support for SipWitchQt.  This includes ruby admin
scripts used to manage the local sqlite3 database of the sipwitchqt server instance.

%description desktop
SipWitchQt desktop client.

%prep
%setup -q

%build
qmake-qt5 CONFIG+=sys_prefix QMAKE_CXXFLAGS+="\"%optflags\"" QMAKE_STRIP="/bin/true"
%{__make} %{?_smp_mflags}

%install
%{__make} install INSTALL_ROOT=%{buildroot}

%post
/sbin/chkconfig --add sipwitchqt

%preun
if [ "$1" = 0 ]; then
	/sbin/service sipwitchqt stop >/dev/null 2>&1
	/sbin/chkconfig --del sipwitchqt
fi	 
exit 0

%postun
if [ "$1" -ge 1 ] ; then
	/sbin/service sipwitchqt condrestart >/dev/null 2>&1
fi
exit 0

%files
%defattr(-,root,root)
%doc README.md DOCKER.md CONTRIBUTING.md LICENSE CHANGELOG
%{_sbindir}/sipwitchqt
%{_sysconfdir}/sv/sipwitchqt/run
%attr(0664,root,root) %config(noreplace) %{_sysconfdir}/sipwitchqt.conf

%files utils
%defattr(-,root,root)
%{_sbindir}/swcert-*
%{_mandir}/man1/swcert-*

%files sqlite3
%defattr(-,root,root)
%{_sbindir}/swlite-*
%{_mandir}/man1/swlite-*

%files desktop
%defattr(-,root,root)
%{_bindir}/antisipate

