Summary: Gossip VMOD for Varnish
Name: vmod-gossip
Version: 4.0
Release: 1%{?dist}
License: BSD
URL: https://github.com/carlosabalde/libvmod-gossip
Group: System Environment/Daemons
Source0: libvmod-gossip.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Requires: varnish >= 6.2.0
BuildRequires: make, python-docutils, varnish >= 6.2.0, varnish-devel >= 6.2.0

%description
Gossip VMOD for Varnish

%prep
%setup -n libvmod-gossip

%build
./autogen.sh
./configure --prefix=/usr/ --docdir='${datarootdir}/doc/%{name}' --libdir='%{_libdir}'
%{__make}
%{__make} check

%install
[ %{buildroot} != "/" ] && %{__rm} -rf %{buildroot}
%{__make} install DESTDIR=%{buildroot}

%clean
[ %{buildroot} != "/" ] && %{__rm} -rf %{buildroot}

%files
%defattr(-,root,root,-)
%{_libdir}/varnish*/vmods/
%doc /usr/share/doc/%{name}/*
%{_mandir}/man?/*

%changelog
* Mon Mar 18 2019 Carlos Abalde <carlos.abalde@gmail.com> - 4.0-1.20190318
- Migrated to Varnish Cache 6.2.x.
* Tue Sep 18 2018 Carlos Abalde <carlos.abalde@gmail.com> - 3.0-1.20180918
- Migrated to Varnish Cache 6.1.x.
* Sat May 12 2018 Carlos Abalde <carlos.abalde@gmail.com> - 1.0-1.20180512
- Initial version.
