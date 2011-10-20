# $Id$

Name:		slash2-mds
Version:	18207
Release:	1%{?dist}
Summary:	PSC's SLASH2 file system's metadata utilities

Group:		File systems
License:
URL:		http://www.psc.edu/slash2
Source0:
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:
Requires:	libaio
Requires:	z
Requires:	gcrypt

%description
PSC's SLASH2 file system's metadata utilities

%prep
%setup -q

%build
%configure
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT

mkdir -p $RPM_BUILD_ROOT/var/lib/slash

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc
/usr/local/sbin/slashd
/usr/local/sbin/slimmns_format
/usr/local/sbin/slkeymgt
/usr/local/sbin/slmctl
/usr/local/sbin/slmkjrnl
/usr/local/sbin/zdb
/usr/local/sbin/zfs
/usr/local/sbin/zfs-fuse
/usr/local/sbin/zpool
/usr/local/sbin/zstreamdump
/usr/local/sbin/ztest
/var/lib/slash
%_mandir/man*/*

%changelog
