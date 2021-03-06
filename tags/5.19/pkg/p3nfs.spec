%define version  5.19
%define rel      1

Summary: Mount Epoc / SymbianOS filesystems
Name: p3nfs
Version: %version
Release: %rel
Copyright: GPL
Group: Development/Tools
Source: http://www.koeniglich.de/packages/p3nfs-%{version}.tar.gz
BuildRoot: /var/tmp/p3nfs-%{version}
URL: http://www.koeniglich.de/p3nfs.html

%description

p3nfsd makes the filesystems of most SymbianOS based PDAs and phones available
to an attached Unix computer. p3nfs talks to the local UNIX machine via NFS,
and uses its own protocol to talk with its client program on the PDA/phone.
The connection can be established via a Serial cable, IrDA, Bluetooth or USB.

%prep
%setup -q

%build
./configure --prefix=%_prefix
make

%install
[ -n "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != / ] && rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/%_prefix/bin $RPM_BUILD_ROOT/%_mandir/man1
mkdir -p $RPM_BUILD_ROOT/%_defaultdocdir/
make DESTDIR="$RPM_BUILD_ROOT" install

%clean
[ -n "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != / ] && rm -rf $RPM_BUILD_ROOT

%files
%defattr(-, root, root)
%attr(4755,root,root) %{_prefix}/bin/p3nfsd
%{_prefix}/bin/*
%{_mandir}/man1/*
%{_defaultdocdir}/p3nfs-%{version}/*


%changelog
* Thu Jan 22 2004 Rudolf Koenig <r_dot_koenig_at_koeniglich.de>
- Changed the specfile to be more (suse)compliant. Thanks to Klaus Singvogel
* Wed Jun  4 2003 Rudolf Koenig <r_dot_koenig_at_koeniglich.de>
- Made SPEC file
