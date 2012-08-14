# $Id$

Name:		slash2-mds
Version:	18221
Release:	1%{?dist}
Summary:	PSC's SLASH2 file system's metadata utilities

Group:		File systems
License:	Propietary
URL:		http://www.psc.edu/slash2
Source0:	dummy.tgz
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

Requires:	libaio
Requires:	z
Requires:	gcrypt

%description
PSC's SLASH2 file system's metadata utilities

%prep
%setup -c
svn co -r %{version} svn+ssh://frodo/cluster/svn/projects .

%build
(cd zfs		&& DEVELPATHS=0 SLASH_MODULES=mds make %{?_smp_mflags})
(cd slash_nara	&& DEVELPATHS=0 SLASH_MODULES=mds make %{?_smp_mflags})

%install
rm -rf $RPM_BUILD_ROOT
(cd zfs		&& INST_BASE=$RPM_BUILD_ROOT/usr/local/psc SLASH_MODULES=mds make install)
(cd slash_nara	&& INST_BASE=$RPM_BUILD_ROOT/usr/local/psc SLASH_MODULES=mds make install)

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc
/usr/local/psc/bin/cursor_mgr
/usr/local/psc/etc/zfs_pool_alert
/usr/local/psc/sbin/slashd
/usr/local/psc/sbin/slmkfs
/usr/local/psc/sbin/slkeymgt
/usr/local/psc/sbin/slmctl
/usr/local/psc/sbin/slmkjrnl
/usr/local/psc/sbin/zdb
/usr/local/psc/sbin/zfs
/usr/local/psc/sbin/zfs-fuse
/usr/local/psc/sbin/zpool
/usr/local/psc/sbin/zstreamdump
/usr/local/psc/sbin/ztest
/usr/local/psc/man/*/*

%changelog
