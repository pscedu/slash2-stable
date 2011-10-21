# $Id$

Name:		slash2-io
Version:	18207
Release:	1%{?dist}
Summary:	PSC's SLASH2 file system's I/O utilities

Group:		File systems
License:	Propietary
URL:		http://www.psc.edu/slash2
Source0:
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:
Requires:

%description
PSC's SLASH2 file system's I/O utilities

%prep
%setup -c
svn co -r %{version} svn+ssh://frodo/cluster/svn/projects .

%build
cd slash_nara
DEVELPATHS=0 SLASH_MODULES=io make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
cd slash_nara
INST_BASE=$RPM_BUILD_ROOT/usr/local/psc make install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc
/usr/local/psc/sbin/sliod
/usr/local/psc/sbin/slictl
/usr/local/psc/sbin/slkeymgt
/usr/local/psc/sbin/slimmns_format
/usr/local/psc/man/*/*

%changelog
