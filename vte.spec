Name: vte
Version: 0.1
Release: 1
Summary: A terminal emulator for testing.
License: LGPL
Group: User Interface/X
BuildRoot: %{_tmppath}/%{name}-root
Source: %{name}-%{version}.tar.gz

%description
Blah.

%package devel
Summary: Development files for use with vte.
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}

%description devel
Blah

%prep
%setup -q

%build
%configure --enable-shared --enable-static
make

%clean
rm -fr $RPM_BUILD_ROOT

%install
rm -fr $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root)
%doc COPYING HACKING NEWS README
%{_bindir}/*
%{_libdir}/*.so.*.*

%files devel
%defattr(-,root,root)
%{_includedir}/*
%{_libdir}/*.a
%{_libdir}/*.so
