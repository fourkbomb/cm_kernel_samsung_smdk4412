%define config_name trats2_defconfig
%define buildarch arm
%define target_board trats2
%define variant %{buildarch}-%{target_board}

Name: linux-3.0
Summary: The Linux Kernel
Version: 3.0.101
Release: 0
License: GPL-2.0
Group: System Environment/Kernel
Vendor: The Linux Community
URL: http://www.kernel.org
Source0: linux-3.0.101.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{PACKAGE_VERSION}-root
Provides: linux-3.0.101
%define __spec_install_post /usr/lib/rpm/brp-compress || :
%define debug_package %{nil}

%define fullVersion %{version}-%{config_name}

BuildRequires:  cpio
BuildRequires:  lzma
BuildRequires:  python
BuildRequires:  binutils-devel
BuildRequires:  lzop
BuildRequires:  module-init-tools
%ifarch %arm
BuildRequires:  u-boot-tools
%endif
ExclusiveArch:	%arm

%if "%{?tizen_profile_name}" != "mobile"
ExcludeArch: %{arm}
%endif

%package -n %{variant}-linux-kernel
Summary: Tizen kernel
Group: System/Kernel
Provides: kernel-uname-r = %{fullVersion}

%description
The Linux Kernel, the operating system core itself

%description -n %{variant}-linux-kernel
This package contains the Linux kernel for Tizen (%{profile} profile, arch %{buildarch}, target board %{target_board})

%package -n %{variant}-linux-kernel-headers
Summary:        Linux support headers for userspace development
Group:          Development/System

%description -n %{variant}-linux-kernel-headers
This package provides userspaces headers from the Linux kernel.  These
headers are used by the installed headers for GNU glibc and other system
 libraries.

%package -n %{variant}-linux-kernel-modules
Summary:	Linux kernel modules
Group:          Development/System
Provides: kernel-modules = %{fullVersion}
Provides: kernel-modules-uname-r = %{fullVersion}

%description -n %{variant}-linux-kernel-modules
This package provides kernel modules.

%package -n %{variant}-linux-kernel-devel
Summary:        Prebuild Linux kernel
Group:          Development/System
Provides: kernel-devel = %{fullVersion}
Provides: kernel-devel-uname-r = %{fullVersion}

%description -n %{variant}-linux-kernel-devel
Prebuild linux kernel

%package -n %{variant}-linux-kernel-debug
Summary:	Debug package for %{variant} kernel
Group:          Development/System

%description -n %{variant}-linux-kernel-debug
Debug package for %{variant} kernel

%prep
%setup -q

%build
# 1. Compile sources
# Make sure EXTRAVERSION says what we want it to say
sed -i "s/^EXTRAVERSION.*/EXTRAVERSION = -%{config_name}/" Makefile

make %{config_name}
make %{?_smp_mflags} uImage

# 2. Build modules
make modules %{?_smp_mflags}

# 4. Create tar repo for build directory
tar cpf linux-kernel-build-%{fullVersion}.tar .

%install

QA_SKIP_BUILD_ROOT="DO_NOT_WANT"; export QA_SKIP_BUILD_ROOT

# 1. Destination directories
mkdir -p %{buildroot}/usr/src/linux-kernel-build-%{fullVersion}

mkdir -p %{buildroot}/boot/
mkdir -p %{buildroot}/lib/modules/%{fullVersion}
mkdir -p %{buildroot}/var/tmp/kernel/

# 2. Install zImage, System.map
install -m 755 arch/arm/boot/uImage %{buildroot}/boot/
install -m 644 System.map %{buildroot}/boot/System.map-%{fullVersion}
install -m 644 .config %{buildroot}/boot/config-%{fullVersion}
install -m 755 arch/arm/boot/uImage %{buildroot}/var/tmp/kernel/
install -m 644 vmlinux %{buildroot}/var/tmp/kernel/
install -m 644 .config %{buildroot}/var/tmp/kernel/config
install -m 644 System.map %{buildroot}/var/tmp/kernel/System.map

# 3. Install modules
make -j8 INSTALL_MOD_STRIP=1 INSTALL_MOD_PATH=%{buildroot}/ modules_install KERNELRELEASE=%{fullVersion}

# 4. Install kernel headers
make -j8 INSTALL_PATH=%{buildroot} INSTALL_MOD_PATH=%{buildroot} INSTALL_HDR_PATH=%{buildroot}/usr headers_install

# 5. Restore source and build irectory
tar -xf linux-kernel-build-%{fullVersion}.tar -C %{buildroot}/usr/src/linux-kernel-build-%{fullVersion}
#ls %{buildroot}/usr/src/linux-kernel-build-%{fullVersion}
#ls %{buildroot}/usr/src/linux-kernel-build-%{fullVersion}/arch
#ls %{buildroot}/usr/src/linux-kernel-build-%{fullVersion}/arch/%{buildarch}
#mv %{buildroot}/usr/src/linux-kernel-build-%{fullVersion}/arch/%{buildarch} .
#mv %{buildroot}/usr/src/linux-kernel-build-%{fullVersion}/arch/Kconfig .
#rm -rf %{buildroot}/usr/src/linux-kernel-build-%{fullVersion}/arch/*
#mv %{buildarch} %{buildroot}/usr/src/linux-kernel-build-%{fullVersion}/arch/
#mv Kconfig      %{buildroot}/usr/src/linux-kernel-build-%{fullVersion}/arch/

# 6. Remove files
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion} -name ".tmp_vmlinux*" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion} -name ".gitignore" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion} -name ".*dtb*tmp" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion} -name "*.*tmp" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion} -name "vmlinux" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion} -name "uImage" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion} -name "zImage" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion} -name "test-*" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion} -name "*.cmd" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion} -name "*.ko" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion} -name "*.o" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion} -name "*.S" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion} -name "*.s" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion} -name "*.c" -not -path "%{buildroot}/usr/src/linux-kernel-build-%{fullVersion}/scripts/*" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion} -size 0c -exec rm -f {} \;

find %{buildroot}/usr/include -name "\.install"  -exec rm -f {} \;
find %{buildroot}/usr -name "..install.cmd" -exec rm -f {} \;

# 6.1 Clean Documentation directory
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion}/Documentation -type f ! -name "Makefile" ! -name "*.sh" ! -name "*.pl" -exec rm -f {} \;

rm -rf %{buildroot}/boot/vmlinux*
rm -rf %{buildroot}/System.map*
rm -rf %{buildroot}/vmlinux*

# 7. Update file permisions
%define excluded_files ! -name "*.h" ! -name "*.cocci" ! -name "*.tst" ! -name "*.y" ! -name "*.in" ! -name "*.gperf" ! -name "*.PL" ! -name "lex*" ! -name "check-perf-tracei.pl" ! -name "*.*shipped" ! -name "*asm-generic" ! -name "Makefile*" ! -name "*.lds" ! -name "mkversion" ! -name "zconf.l" ! -name "README" ! -name "*.py" ! -name "gconf.glade" ! -name "*.cc" ! -name "dbus_contexts" ! -name "*.pm" ! -name "*.xs" ! -name "*.l" ! -name "EventClass.py" ! -name "typemap" ! -name "net_dropmonitor.py"

find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion}/tools/perf/scripts/ -type f %{excluded_files} -exec chmod 755 {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion}/scripts/            -type f %{excluded_files} -exec chmod 755 {} \;
find %{buildroot}/usr                                                           -type f ! -name "check-perf-tracei.pl" -name "*.sh" -name "*.pl" -exec chmod 755 {} \;
find %{buildroot}/lib/modules/ -name "*.ko"                                     -type f -exec chmod 755 {} \;

# 8. Create symbolic links
rm -f %{buildroot}/lib/modules/%{fullVersion}/build
rm -f %{buildroot}/lib/modules/%{fullVersion}/source
ln -sf /usr/src/linux-kernel-build-%{fullVersion} %{buildroot}/lib/modules/%{fullVersion}/build

%clean
rm -rf %{buildroot}

%files -n %{variant}-linux-kernel-headers
%defattr(-,root,root)
/usr/include/*

%files -n %{variant}-linux-kernel-devel
%defattr(-,root,root)
/usr/src/linux-kernel-build-%{fullVersion}
/lib/modules/%{fullVersion}/modules.*
/lib/modules/%{fullVersion}/build

%files -n %{variant}-linux-kernel-modules
%defattr(-,root,root)
/lib/modules/%{fullVersion}/kernel
/lib/modules/%{fullVersion}/modules.*

%files -n %{variant}-linux-kernel-debug
%defattr(-,root,root)
/var/tmp/kernel/uImage
/var/tmp/kernel/vmlinux
/var/tmp/kernel/config
/var/tmp/kernel/System.map

%files -n %{variant}-linux-kernel
%defattr(-,root,root)
/boot/System.map*
/boot/config*
/boot/uImage
