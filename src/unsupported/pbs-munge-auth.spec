#
# Copyright (C) 1994-2021 Altair Engineering, Inc.
# For more information, contact Altair at www.altair.com.
#
# This file is part of both the OpenPBS software ("OpenPBS")
# and the PBS Professional ("PBS Pro") software.
#
# Open Source License Information:
#
# OpenPBS is free software. You can redistribute it and/or modify it under
# the terms of the GNU Affero General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version.
#
# OpenPBS is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public
# License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Commercial License Information:
#
# PBS Pro is commercially licensed software that shares a common core with
# the OpenPBS software.  For a copy of the commercial license terms and
# conditions, go to: (http://www.pbspro.com/agreement.html) or contact the
# Altair Legal Department.
#
# Altair's dual-license business model allows companies, individuals, and
# organizations to create proprietary derivative works of OpenPBS and
# distribute them - whether embedded or bundled with other software -
# under a commercial license agreement.
#
# Use of Altair's trademarks, including but not limited to "PBS™",
# "OpenPBS®", "PBS Professional®", and "PBS Pro™" and Altair's logos is
# subject to Altair's trademark licensing policies.

%if !%{defined munge_pbs_socket_name}
%define munge_pbs_socket_name pbs
%endif
%if !%{defined munge_pbs_libauth_name}
%define munge_pbs_libauth_name munge_%{munge_pbs_socket_name}
%endif
%if !%{defined munge_pbs_service_name}
%define munge_pbs_service_name munge-%{munge_pbs_socket_name}
%endif

%if !%{defined pbs_prefix}
%{error:pbs_prefix not defined}
%endif
%if !%{defined pbs_libdir}
%{error:pbs_libdir not defined}
%endif
%if !%{defined pbs_pkg}
%{error:pbs_pkg not defined}
%endif
%if !%{defined pbs_dist}
%{error:pbs_dist not defined}
%endif
%if !%{defined pbs_version}
%{error:pbs_version not defined}
%endif

%if %{defined suse_version}
%define dist .suse%(echo %{suse_version} | sed -e 's/..$//')
%endif

Name: pbs-munge-auth-lib
Version: %{pbs_version}
Release: 10%{?dist}
License: AGPLv3 with exceptions
Summary: User authentication library for PBS using MUNGE
Source: %{pbs_dist}
BuildRoot: %{buildroot}
BuildRequires: gcc
BuildRequires: gcc-c++
BuildRequires: make
BuildRequires: rpm-build
BuildRequires: autoconf
BuildRequires: automake
BuildRequires: libtool
BuildRequires: munge-devel
BuildRequires: %{_sbindir}/munged
BuildRequires: %{_unitdir}/munge.service
BuildRequires: systemd-rpm-macros

%global munge_pbs_socket_path %(%{_sbindir}/munged --help |& grep '[-]-socket=' | \
    %{__sed} -e 's,.*\\[\\(.*\\)\\(/[^.]*\\)\\(\\.[^]]*\\)\\],\\1\\2-%{munge_pbs_socket_name}\\3,')
%global munge_pbs_key_path %(%{_sbindir}/munged --help |& grep '[-]-key-file=' | \
    %{__sed} -e 's,.*\\[\\(.*\\)\\(/[^.]*\\)\\(\\.[^]]*\\)\\],\\1\\2-%{munge_pbs_socket_name}\\3,')
%global munge_pbs_pid_path %(%{_sbindir}/munged --help |& grep '[-]-pid-file=' | \
    %{__sed} -e 's,.*\\[\\(.*\\)\\(/[^.]*\\)\\(\\.[^]]*\\)\\],\\1\\2-%{munge_pbs_socket_name}\\3,')

%description
This package provides an authentication library for OpenPBS and PBS Pro
that ulilizes a MUNGE service listening on a PBS specific socket.

    Socket: %{munge_pbs_socket_path}

To make use of this library, add the following settings to the PBS
configuration file, typically /etc/pbs.conf, on each node.

    PBS_SUPPORTED_AUTH_METHODS=munge_%{munge_pbs_socket_name}
    PBS_AUTH_METHOD=munge_%{munge_pbs_socket_name}

You may also wish to install the %{munge_pbs_service_name}-service package, which
contains a preconfigured systemd service with the correct path settings.

%prep
%setup -n %{pbs_pkg}-%{pbs_version}

%build
[ -x configure ] || ./autoconf
%{_configure} PBS_VERSION=%{pbs_version} --prefix=%{pbs_prefix} \
    --with-munge-pbs-socket=%{munge_pbs_socket_name} \
    --with-munge-pbs-libauth-name=%{munge_pbs_libauth_name}
cd src/lib/Libauth/munge
%{make_build}
cd -
if [ "%{munge_pbs_service_name}" != "munge" ] ; then
    cd src/unsupported
    munged_options=$(%{_sbindir}/munged --help |& \
        %{__sed} -n -e '/=PATH/ s/.*\(--[^=]*=\)PATH.*\[\([^]]*\)\]/\1\2/ p' | \
        %{__sed} -e 's,\(.*\)\(/[^.]*\)\(\..*\),\1\2-%{munge_pbs_socket_name}\3,' | \
        tr '\n' ' ' | %{__sed} -e 's/ $/\n/')
    %{__sed} -e 's,\(ExecStart=\([^ ]\|$\)*\).*,\1 '"$munged_options"',' \
        -e 's,\(PIDFile=\).*,\1'"%{munge_pbs_pid_path}"',' \
        -e '/EnvironmentFile=/ d' \
    %{_unitdir}/munge.service >%{munge_pbs_service_name}.service
    [ -z "%{munge_pbs_pid_path}" ] && \
        %{__sed} -i -e '/PIDFile=/ d' %{munge_pbs_service_name}.service
    cd -
fi

%install
cd src/lib/Libauth/munge
%{make_install}
cd -
if [ "%{munge_pbs_service_name}" != "munge" ] ; then
    cd src/unsupported
    mkdir -p -m 755 %{?buildroot}/%{_unitdir}
    %{__install} -m 444 -t %{?buildroot}/%{_unitdir} \
        %{munge_pbs_service_name}.service
    cd -
fi

%files
%defattr(555, root, root, 755)
%{pbs_libdir}/libauth_%{munge_pbs_libauth_name}.so*
%exclude %{pbs_libdir}/libauth_%{munge_pbs_libauth_name}.a
%exclude %{pbs_libdir}/libauth_%{munge_pbs_libauth_name}.la

%if "%{munge_pbs_service_name}" != "munge"
%package -n %{munge_pbs_service_name}-service
Summary: MUNGE service for PBS user authentication
Requires: %{_sbindir}/munged

%description -n %{munge_pbs_service_name}-service
This package contains a systemd service definition for MUNGE that uses a
socket specifically for OpenPBS and PBS Pro.  The associated library for
PBS can be found in the pbs-munge-auth-lib package.  The key and socket
paths used by the service are as follows.

    Key File: %{munge_pbs_key_path}
    Socket: %{munge_pbs_socket_path}

The key file must contain the same contents on all nodes in the PBS
complex and should be set prior to starting this service.

%files -n %{munge_pbs_service_name}-service
%defattr(444, root, root, 755)
%{_unitdir}/%{munge_pbs_service_name}.service

%post -n %{munge_pbs_service_name}-service
%systemd_post %{munge_pbs_service_name}.service

%preun -n %{munge_pbs_service_name}-service
%systemd_preun %{munge_pbs_service_name}.service

%postun -n %{munge_pbs_service_name}-service
%systemd_postun_with_restart %{munge_pbs_service_name}.service
%endif # "%{munge_pbs_service_name}" != "munge"
