# $Id$

Name:		pfl
Version:	18221
Release:	1%{?dist}
Summary:	Pittsburgh Supercomputing's file system libraries and toolset

Group:		File systems
License:	Propietary
URL:		http://www.psc.edu/advsys
Source0:	dummy.tgz
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%description
Pittsburgh Supercomputing's file system libraries and toolset

%prep
%setup -c
svn co -r %{version} svn+ssh://frodo/cluster/svn/projects .

%build
cd pfl
DEVELPATHS=0 make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
cd pfl
INST_BASE=$RPM_BUILD_ROOT/usr/local/psc make install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc
/usr/local/psc/bin/odtable
/usr/local/psc/sbin/lnrtctl
/usr/local/psc/sbin/lnrtd
/usr/local/psc/man/*/*

%changelog
