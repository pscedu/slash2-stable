# $Id$

Name:		slash2-mds
Version:	1.0
Release:	1%{?dist}
Summary:	SLASH2 file system metadata server (MDS) utilities

Group:		File systems
License:	GPLv2
URL:		http://www.psc.edu/index.php/research-programs/advanced-systems/slash2
Source0:	dummy.tgz
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

Requires:	libaio
Requires:	z
Requires:	gcrypt
Requires:	sqlite3

%description
Pittsburgh Supercomputing Center's SLASH2 distributed file system
metadata server (MDS) utilities.

%prep
%setup -c
git clone https://github.com/pscedu/pfl projects
cd projects
make scm-fetch:slash2

%build
(cd slash2	&& SLASH_MODULES=mds make %{?_smp_mflags})

%install
rm -rf $RPM_BUILD_ROOT
(cd zfs		&& INST_BASE=$RPM_BUILD_ROOT/usr/local/psc SLASH_MODULES=mds make install)
(cd slash2	&& INST_BASE=$RPM_BUILD_ROOT/usr/local/psc SLASH_MODULES=mds make install)

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc
/usr/local/psc/bin/cursor_mgr
/usr/local/psc/etc/zfs_pool_alert
/usr/local/psc/man/*/*
/usr/local/psc/sbin/dumpfid
/usr/local/psc/sbin/odtable
/usr/local/psc/sbin/pfl_daemon.sh
/usr/local/psc/sbin/reclaim
/usr/local/psc/sbin/slash2_check
/usr/local/psc/sbin/slash2_check.py
/usr/local/psc/sbin/slashd
/usr/local/psc/sbin/slashd.sh
/usr/local/psc/sbin/slkeymgt
/usr/local/psc/sbin/slmctl
/usr/local/psc/sbin/slmkfs
/usr/local/psc/sbin/slmkjrnl
/usr/local/psc/sbin/zdb
/usr/local/psc/sbin/zfs
/usr/local/psc/sbin/zfs-fuse
/usr/local/psc/sbin/zpool
/usr/local/psc/sbin/zstreamdump
/usr/local/psc/sbin/ztest

%changelog
