# $Id$

Name:		slash2-io
Version:	1.0
Release:	1%{?dist}
Summary:	SLASH2 file system I/O server (IOS) utilities

Group:		File systems
License:	GPLv2
URL:		http://www.psc.edu/index.php/research-programs/advanced-systems/slash2
Source0:	dummy.tgz
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%description
Pittsburgh Supercomputing Center's SLASH2 distributed file system
I/O server (IOS) utilities.

%prep
%setup -c
git clone https://github.com/pscedu/pfl projects
cd projects
make scm-fetch:slash2

%build
cd slash2
SLASH_MODULES=ion make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
cd slash2
INST_BASE=$RPM_BUILD_ROOT/usr/local/psc SLASH_MODULES=ion make install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc
/usr/local/psc/man/*/*
/usr/local/psc/sbin/fshealthtest
/usr/local/psc/sbin/pfl_daemon.sh
/usr/local/psc/sbin/slash2_check
/usr/local/psc/sbin/slash2_check.py
/usr/local/psc/sbin/slictl
/usr/local/psc/sbin/slictlN
/usr/local/psc/sbin/sliod
/usr/local/psc/sbin/sliod.sh
/usr/local/psc/sbin/slkeymgt
/usr/local/psc/sbin/slmkfs

%changelog
