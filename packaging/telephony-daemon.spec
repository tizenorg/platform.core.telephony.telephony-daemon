%define major 1
%define minor 3
%define patchlevel 37

Name:           telephony-daemon
Version:        %{major}.%{minor}.%{patchlevel}
Release:        1
License:        Apache-2.0
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
	-DUNIT_INSTALL_DIR=%{_unitdir} \
	-DVERSION=%{version} \
	-DTIZEN_DEBUG_ENABLE=1 \
%if "%{?tizen_profile_name}" == "tv"
	-DTIZEN_PROFILE_TV=1 \
%endif

make %{?_smp_mflags}

%install

%make_install
%if "%{?tizen_profile_name}" != "tv"
mkdir -p %{buildroot}%{_unitdir}/multi-user.target.wants
ln -s %{_unitdir}/telephony-daemon.service %{buildroot}%{_unitdir}/multi-user.target.wants/telephony-daemon.service
%endif

%post -p /sbin/ldconfig

%files
%manifest telephony-daemon.manifest
%defattr(644,system,system,-)
%caps(cap_mac_override,cap_dac_override,cap_net_admin=eip) %attr(755,system,system) %{_bindir}/telephony-daemon
%{_unitdir}/telephony-daemon.service
%if "%{?tizen_profile_name}" != "tv"
%{_unitdir}/multi-user.target.wants/telephony-daemon.service
%endif
%{_datadir}/license/telephony-daemon
