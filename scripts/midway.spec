Summary: The MidWay service request broker
Name: MidWay
Version: 0.10.2
Release: 1
Copyright: GPL,LGPL
Group: System Environment/Base
Source: ftp://ftp.mid-way.org/pub/sourceforge/MidWay/midway-%{version}.tgz
URL: http://www.mid-way.org

BuildRoot: /var/tmp/%{name}-buildroot

%description
MidWay is a service request broker
%prep
%setup -q

%build
configure --prefix=${RPM_BUILD_ROOT}/usr 
make

%install
rm -rf $RPM_BUILD_ROOT
make install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc README NOTES Changelog html-doc

/usr/bin/*
/usr/include/*
/usr/lib/*
/usr/man/*

%changelog
