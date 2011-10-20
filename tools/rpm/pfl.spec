# $Id$

Name:		pfl
Version:	18207
Release:	1%{?dist}
Summary:	Pittsburgh Supercomputing's file system libraries and toolset

Group:		File systems
License:
URL:		http://www.psc.edu/advsys
Source0:
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:
Requires:

%description
Pittsburgh Supercomputing's file system libraries and toolset

%prep
%setup -q
svn co svn+ssh://frodo/cluster/svn/projects .
perl -i -pe 's/.*(?:SLASH|ZEST)/#$&/' Makefile

%build
DEVELPATHS=0 make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
INST_BASE=$RPM_BUILD_ROOT make install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc
/usr/local/bin/fio
/usr/local/bin/odtable
/usr/local/bin/sft
/usr/local/sbin/lnrtctl
/usr/local/sbin/lnrtd
%_mandir/man*/*

%changelog
