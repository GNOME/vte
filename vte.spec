Name: vte
Version: 0.1
Release: 1
Summary: A terminal emulator for testing.
License: LGPL
Group: User Interface/X
BuildRoot: %{_tmppath}/%{name}-root
Source: %{name}-%{version}.tar.gz

%description
VTE is an experimental terminal emulator widget.  This package contains
a sample application which uses the widget in a terminal window.

%package devel
Summary: Files needed for developing applications which use vte.
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}

%description devel
VTE is an experimental terminal emulator widget.  This package contains
the files needed for building applications using the widget.

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
%{_libdir}/pkgconfig/*
