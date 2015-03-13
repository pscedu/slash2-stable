# $Id$

Name:		slash2-client
Version:	18221
Release:	1%{?dist}
Summary:	PSC's SLASH2 file system's client utilities

Group:		File systems
License:	Propietary
URL:		http://www.psc.edu/slash2
Source0:	dummy.tgz
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%description
PSC's SLASH2 file system's client utilities

%prep
%setup -c
svn co -r %{version} svn+ssh://frodo/cluster/svn/projects .

%build
cd slash2
DEVELPATHS=0 SLASH_MODULES=cli make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
cd slash2
INST_BASE=$RPM_BUILD_ROOT/usr/local/psc SLASH_MODULES=cli make install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc
/usr/local/psc/sbin/mount_slash
/usr/local/psc/sbin/msctl
/usr/local/psc/sbin/slkeymgt
/usr/local/psc/man/*/*

%changelog
