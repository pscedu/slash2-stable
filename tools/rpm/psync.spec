# $Id$

Name:		psync
Version:	1.0
Release:	1%{?dist}
Summary:	parallel rsync-clone

Group:		File transfer utilities
License:	ISC
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
make scm-fetch:psync

%build
cd psync
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
cd psync
INST_BASE=$RPM_BUILD_ROOT/usr/local/psc make install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc
/usr/local/psc/man/*/*
/usr/local/psc/bin/psync

%changelog
