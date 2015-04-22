%define major 1
%define minor 3
%define patchlevel 24

Name:           telephony-daemon
Version:        %{major}.%{minor}.%{patchlevel}
Release:        1
License:        Apache
Summary:        Telephony Daemon
Group:          System/Telephony
Source0:        %{name}-%{version}.tar.gz
BuildRequires:  cmake
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(tcore)
BuildRequires:  pkgconfig(vconf)

%description
Description: Telephony Daemon

%prep
%setup -q

%build
cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix} \
	-DLIB_INSTALL_DIR=%{_libdir} \
	-DVERSION=%{version} \
	-DTIZEN_DEBUG_ENABLE=1 \

make %{?_smp_mflags}

%install

%make_install
mkdir -p %{buildroot}%{_libdir}/systemd/system/multi-user.target.wants
ln -s %{_libdir}/systemd/system/telephony-daemon.service %{buildroot}%{_libdir}/systemd/system/multi-user.target.wants/telephony-daemon.service

%post -p /sbin/ldconfig

%files
%manifest telephony-daemon.manifest
%defattr(-,root,root,-)
%{_bindir}/telephony-daemon
%attr(644,root,root) %{_libdir}/systemd/system/telephony-daemon.service
%attr(644,root,root) %{_libdir}/systemd/system/multi-user.target.wants/telephony-daemon.service
%{_datadir}/license/telephony-daemon
