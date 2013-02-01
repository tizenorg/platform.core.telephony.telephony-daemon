Name:       telephony-daemon
Summary:    Telephony daemon
Version:    0.1.12
Release:    2
Group:      System/Telephony
License:    Apache
Source0:    %{name}-%{version}.tar.gz
Source1:    telephony.service
BuildRequires:  cmake
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gobject-2.0)
BuildRequires:  pkgconfig(tcore)
BuildRequires:  pkgconfig(dlog)

%description
Description: Telephony daemon

%prep
%setup -q

%build
%cmake . -DVERSION=%{version}
make %{?jobs:-j%jobs}

%install
%make_install
mkdir -p %{buildroot}%{_prefix}/lib/systemd/system/multi-user.target.wants
install -m 0644 %{SOURCE1} %{buildroot}%{_prefix}/lib/systemd/system/telephony.service
ln -s ../telephony.service %{buildroot}%{_prefix}/lib/systemd/system/multi-user.target.wants/telephony.service
mkdir -p %{buildroot}/usr/share/license

%files
%manifest telephony-daemon.manifest
%defattr(-,root,root,-)
%{_bindir}/telephony-daemon
%{_sysconfdir}/rc.d/init.d/telephony-daemon
%{_sysconfdir}/rc.d/rc3.d/S30telephony-daemon
%{_sysconfdir}/rc.d/rc5.d/S30telephony-daemon
%{_prefix}/lib/systemd/system/telephony.service
%{_prefix}/lib/systemd/system/multi-user.target.wants/telephony.service
/usr/share/license/telephony-daemon
