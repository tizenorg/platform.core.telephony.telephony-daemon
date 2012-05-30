Name:       telephony-daemon
Summary:    Telephony daemon
Version:    0.1.3
Release:    1
Group:      System/Telephony
License:    Apache
Source0:    %{name}-%{version}.tar.gz
Source1001: packaging/telephony-daemon.manifest 
BuildRequires:  cmake
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gobject-2.0)
BuildRequires:  pkgconfig(tcore)
BuildRequires:  pkgconfig(dlog)

%description
Description: Telephony daemon

%prep
%setup -q
cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix}

%build
cp %{SOURCE1001} .
make %{?jobs:-j%jobs}

%install
%make_install

%files
%manifest telephony-daemon.manifest
%defattr(-,root,root,-)
%{_bindir}/telephony-daemon
%{_initrddir}/telephony-daemon
%{_sysconfdir}/rc.d/rc3.d/S30telephony-daemon
%{_sysconfdir}/rc.d/rc5.d/S30telephony-daemon
