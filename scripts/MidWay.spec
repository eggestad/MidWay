# -*- RPM-SPEC -*- 
Summary: The MidWay service request broker
Name: MidWay
Version: 0.14.0
Release: 1
License: GPL,LGPL
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
if [[ $1 > 1 ]]; then 
  service mwbd restart	
else 
  chkconfig --add mwbd
  service mwbd start	
fi

%preun
if [[ 0 == $1 ]]; then
  service mwbd stop
  chkconfig --del mwbd
fi

 
%files
%defattr(-,root,root)
%doc README NOTES ChangeLog html-doc

%attr(555,root,root) /etc/rc.d/init.d/mwbd
/usr/bin/*
/usr/sbin/*
${exec_prefix}/lib64/libMidWay.so.0
${exec_prefix}/lib64/libMidWay.so.0.14
/usr/man/man1/*
/usr/man/man7/*
/usr/man/man8/*


%files devel
%defattr(-,root,root)
/usr/man/man3/*
/usr/include/MidWay.h
${exec_prefix}/lib64/libMidWay.so
${exec_prefix}/lib64/libMidWay.a
${exec_prefix}/lib64/liburlencode.a

%changelog
