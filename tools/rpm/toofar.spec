# $Id$

Name:		toofar
Version:	18207
Release:	1%{?dist}
Summary:	PSC's archiver user interface

Group:		File systems
License:	Propietary
URL:		http://www.psc.edu/advsys
Source0:	dummy.tgz
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%description
PSC's archiver user interface

%prep
%setup -c
svn co -r %{version} svn+ssh://frodo/cluster/svn/projects .

%build
cd apps/arc/far
DEVELPATHS=0 make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
cd apps/arc/far
INST_BASE=$RPM_BUILD_ROOT/usr/local/psc make install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc
/usr/local/psc/bin/far
/usr/local/psc/man/*/*

%changelog
