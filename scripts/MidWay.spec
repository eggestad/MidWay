# -*- RPM-SPEC -*- 
Summary: The MidWay service request broker
Name: MidWay
Version: 0.12.0
Release: 1
Copyright: GPL,LGPL
Group: System Environment/Base
Source: ftp://ftp.mid-way.org/pub/sourceforge/MidWay/MidWay-%{version}.tgz
URL: http://www.mid-way.org

BuildRoot: /var/tmp/%{name}-buildroot

%description
MidWay is a service request broker. It provides a simple way for server processes to provide services, 
and clients to call these services. It's primary design goal is hig performance. 
It also provides a session level environment handling users, and authentication.

%package devel
Summary: Development envronment for MidWay
Group: System Environment/Base
Requires: MidWay = %{version}

%description devel
This is the development part of midway, thats the include file, the libraries, 
and man pages for the library. 

%prep
%setup -q

%build
configure --prefix=${RPM_BUILD_ROOT}/usr 
make

%install
rm -rf $RPM_BUILD_ROOT
make install
mkdir ${RPM_BUILD_ROOT}/etc
mkdir ${RPM_BUILD_ROOT}/etc/rc.d
mkdir ${RPM_BUILD_ROOT}/etc/rc.d/init.d
sed s+${RPM_BUILD_ROOT}++g ${RPM_BUILD_ROOT}/usr/sbin/midway.rc > ${RPM_BUILD_ROOT}/etc/rc.d/init.d/mwbd

%clean
rm -rf $RPM_BUILD_ROOT

%post 
ln -s /usr/lib/libMidWay.so.0.12 /usr/lib/libMidWay.so.0
chkconfig --add mwbd
service mwbd start

%preun
service mwbd stop
chkconfig --del mwbd
rm -f /usr/lib/libMidWay.so.0

%post devel
ln -s /usr/lib/libMidWay.so.0.12 /usr/lib/libMidWay.so

%preun devel 
rm -f /usr/lib/libMidWay.so
 
%files
%defattr(-,root,root)
%doc README NOTES ChangeLog html-doc

%attr(555,root,root) /etc/rc.d/init.d/mwbd
/usr/bin/*
/usr/sbin/*
/usr/lib/libMidWay.so.0.12
/usr/man/man1/*
/usr/man/man7/*
/usr/man/man8/*

%files devel
/usr/man/man3/*
/usr/include/MidWay.h
/usr/lib/libMidWay.a
/usr/lib/liburlencode.a

%changelog
