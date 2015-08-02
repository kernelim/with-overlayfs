Name:           PKG_NAME
Version:        PKG_VERSION
Release:        PKG_RELEASE%{?dist}
Summary:        Execution wrapper for private volatile global path modifications
License:        Apache License 2.0
URL:            https://github.com/kernelim/with-overlayfs
Source0:        PKG_NAME-PKG_VERSION.tar.gz

BuildRequires:  gcc-c++
BuildRequires:  automake
BuildRequires:  autoconf
BuildRequires:  libtool
BuildRequires:  pandoc

%description
Execution wrapper for private volatile global path modifications

%prep
%setup -q

%build

./autogen.sh
./configure \
    --prefix=/usr \
    --libdir=%{_libdir} \

make %{?_smp_mflags}

%check

%install

rm -rf %{buildroot}
make install DESTDIR=%{buildroot}

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%attr(4755,root,root) %{_bindir}/with-overlayfs
%{_libdir}/libwith_overlayfs*
%{_datadir}/with-overlayfs/*
%{_datadir}/man/*

%changelog
