# $Id$

Name:		sft
Version:	1.0
Release:	1%{?dist}
Summary:	"Swiss army" file tool

Group:		File utilities
License:	ISC
URL:		http://www.psc.edu/index.php/research-programs/advanced-systems/slash2
Source0:	dummy.tgz
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%description
Pittsburgh Supercomputing Center's "Swiss army" file tool for various
file system testing/stressing operations.

%prep
%setup -c
git clone https://github.com/pscedu/pfl projects
cd projects
make scm-fetch:sft

%build
cd sft
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
cd sft
INST_BASE=$RPM_BUILD_ROOT/usr/local/psc make install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc
/usr/local/psc/man/*/*
/usr/local/psc/bin/sft

%changelog
