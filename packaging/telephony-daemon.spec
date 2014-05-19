%define major 3
%define minor 0
%define patchlevel 1

Name:       telephony-daemon
Version:        %{major}.%{minor}.%{patchlevel}
Release:        1
License:        Apache-2.0
Summary:    Telephony daemon
Group:      System/Telephony
Source0:    %{name}-%{version}.tar.gz
Source1001: 	telephony-daemon.manifest
BuildRequires:  cmake
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gobject-2.0)
BuildRequires:  pkgconfig(tcore)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(libsystemd-daemon)
BuildRequires:  pkgconfig(tel-headers)
Requires(post):           sys-assert
%{?systemd_requires}

%description
Description: Telephony daemon

%prep
%setup -q
cp %{SOURCE1001} .

%build
%cmake . -DVERSION=%{version}
make %{?jobs:-j%jobs}

%install
%make_install
mkdir -p %{buildroot}%{_prefix}/lib/systemd/system/multi-user.target.wants
ln -s ../telephony.service %{buildroot}%{_prefix}/lib/systemd/system/multi-user.target.wants/telephony.service
mkdir -p %{buildroot}/usr/share/license
cp LICENSE %{buildroot}/usr/share/license/%{name}

%files
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_bindir}/telephony-daemon
%{_prefix}/lib/systemd/system/telephony.service
%{_prefix}/lib/systemd/system/multi-user.target.wants/telephony.service
/usr/share/license/%{name}
