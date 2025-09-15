Summary: Gossip VMOD for Varnish
Name: vmod-gossip
Version: 17.0
Release: 1%{?dist}
License: BSD
URL: https://github.com/carlosabalde/libvmod-gossip
Group: System Environment/Daemons
Source0: libvmod-gossip.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Requires: varnish >= 8.8.0
BuildRequires: make, python-docutils, varnish >= 8.8.0, varnish-devel >= 8.8.0

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
%{_libdir}/varnish*/vmods/lib*
%doc /usr/share/doc/%{name}/*
%{_mandir}/man?/*

%changelog
* Mon Sep 15 2025 Carlos Abalde <carlos.abalde@gmail.com> - 17.0-1.20250915
- Migrated to Varnish Cache 8.0.x.
* Tue Mar 18 2025 Carlos Abalde <carlos.abalde@gmail.com> - 16.0-1.20250318
- Migrated to Varnish Cache 7.7.x.
* Fri Sep 13 2024 Carlos Abalde <carlos.abalde@gmail.com> - 15.0-1.20240913
- Migrated to Varnish Cache 7.6.x.
* Tue Mar 19 2024 Carlos Abalde <carlos.abalde@gmail.com> - 14.0-1.20240319
- Migrated to Varnish Cache 7.5.x.
* Fri Sep 15 2023 Carlos Abalde <carlos.abalde@gmail.com> - 13.0-1.20230915
- Migrated to Varnish Cache 7.4.x.
* Wed Mar 15 2023 Carlos Abalde <carlos.abalde@gmail.com> - 12.0-1.20230315
- Migrated to Varnish Cache 7.3.x.
* Thu Sep 15 2022 Carlos Abalde <carlos.abalde@gmail.com> - 11.0-1.20220915
- Migrated to Varnish Cache 7.2.x.
* Tue Mar 15 2022 Carlos Abalde <carlos.abalde@gmail.com> - 10.0-1.20220315
- Migrated to Varnish Cache 7.1.x.
* Wed Sep 15 2021 Carlos Abalde <carlos.abalde@gmail.com> - 9.0-1.20210915
- Migrated to Varnish Cache 7.0.x.
* Mon Mar 15 2021 Carlos Abalde <carlos.abalde@gmail.com> - 8.0-1.20210315
- Migrated to Varnish Cache 6.6.x.
* Tue Sep 15 2020 Carlos Abalde <carlos.abalde@gmail.com> - 7.0-1.20200915
- Migrated to Varnish Cache 6.5.x.
* Fri Mar 20 2020 Carlos Abalde <carlos.abalde@gmail.com> - 6.0-1.20200320
- Migrated to Varnish Cache 6.4.x.
* Mon Sep 30 2019 Carlos Abalde <carlos.abalde@gmail.com> - 5.0-1.20190930
- Migrated to Varnish Cache 6.3.x.
* Mon Mar 18 2019 Carlos Abalde <carlos.abalde@gmail.com> - 4.0-1.20190318
- Migrated to Varnish Cache 6.2.x.
* Tue Sep 18 2018 Carlos Abalde <carlos.abalde@gmail.com> - 3.0-1.20180918
- Migrated to Varnish Cache 6.1.x.
* Sat May 12 2018 Carlos Abalde <carlos.abalde@gmail.com> - 1.0-1.20180512
- Initial version.
