Name: libaztec
Version: 1.0.5
Release: 0
Summary: Aztec encoder
Group: Development/Libraries
License: BSD
URL: https://github.com/monich/aztec
Source: %{name}-%{version}.tar.bz2
BuildRequires: pkgconfig(libpng)
BuildRequires: pkgconfig(glib-2.0)
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig

%description
Aztec encoding library.

%package devel
Summary: Development library for %{name}
Requires: %{name} = %{version}
Requires: pkgconfig

%description devel
This package contains the development library for %{name}.

%package -n aztec-tools
Summary: Command line Aztec tools
Group: Applications/File

%description -n aztec-tools
Tools to generate Aztec symbols as image files.

%prep
%setup -q

%build
make KEEP_SYMBOLS=1 release pkgconfig
make -C tools KEEP_SYMBOLS=1 release

%install
rm -rf %{buildroot}
make install-dev DESTDIR=%{buildroot}
make -C tools install DESTDIR=%{buildroot}

%check
make test

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%{_libdir}/%{name}.so.*

%files devel
%defattr(-,root,root,-)
%{_libdir}/pkgconfig/*.pc
%{_libdir}/%{name}.so
%{_includedir}/aztec/*.h

%files -n aztec-tools
%defattr(-,root,root,-)
%{_bindir}/aztec-png
