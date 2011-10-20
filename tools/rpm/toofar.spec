# $Id$

Name:		toofar
Version:	18207
Release:	1%{?dist}
Summary:	PSC's archiver user interface

Group:		File systems
License:
URL:		http://www.psc.edu/advsys
Source0:
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:
Requires:

%description

%prep
%setup -q
svn co svn+ssh://frodo/cluster/svn/projects .

%build
%configure
cd apps/arc/far
DEVELPATHS=0 make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
cd apps/arc/far
INST_BASE=$RPM_BUILD_ROOT make install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc
/usr/local/bin/far
%_mandir/man*/*

%changelog
