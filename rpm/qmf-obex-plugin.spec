Name:		qmf-obex-plugin
Summary:	OBEX plugin for Qt Messaging Framework (QMF)
Version:	0.0.1
Release:	1
Group:		System/Libraries
License:	BSD
URL:		https://git.merproject.org/ruff/qmf-obex-plugin
Source0:	%{name}-%{version}.tar.bz2
Requires:       libqmfmessageserver1-qt5
BuildRequires:	pkgconfig(Qt5Core)
BuildRequires:	pkgconfig(qmfclient5)
BuildRequires:	pkgconfig(qmfmessageserver5)
#BuildRequires:	pkgconfig(accounts-qt5) >= 1.13

%description
OBEX plugin for Qt Messaging Framework (QMF) exposing QMail over DBus

%prep
%setup -q -n %{name}-%{version}

%build

%qmake5 QMF_INSTALL_ROOT=/usr

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%qmake5_install

%files
%defattr(-,root,root,-)
%{_libdir}/qmf/plugins5/messageserverplugins/libobexdbus.so

