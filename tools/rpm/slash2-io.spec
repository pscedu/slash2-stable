# $Id$

Name:		slash2-io
Version:	18207
Release:	1%{?dist}
Summary:	PSC's SLASH2 file system's I/O utilities

Group:		File systems
License:
URL:		http://www.psc.edu/slash2
Source0:
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:
Requires:

%description
PSC's SLASH2 file system's I/O utilities

%prep
%setup -q
svn co svn+ssh://frodo/cluster/svn/projects .

%build
%configure
cd slash_nara
DEVELPATHS=0 SLASH_MODULES=io make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
cd slash_nara
INST_BASE=$RPM_BUILD_ROOT make install

mkdir -p $RPM_BUILD_ROOT/var/lib/slash

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc
/usr/local/sbin/sliod
/usr/local/sbin/slictl
/usr/local/sbin/slkeymgt
/usr/local/sbin/slimmns_format
/var/lib/slash
%_mandir/man*/*

%changelog
