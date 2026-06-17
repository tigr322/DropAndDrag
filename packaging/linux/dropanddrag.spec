Name:           dropanddrag
Version:        1.0.0
Release:        1%{?dist}
Summary:        Fast cross-platform drag-and-drop shelf utility
License:        MIT
URL:            https://github.com/dropanddrag/dropanddrag
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.25
BuildRequires:  gcc-c++ >= 13
BuildRequires:  sqlite-devel >= 3.30
BuildRequires:  libX11-devel
BuildRequires:  libwayland-devel
BuildRequires:  mesa-libGL-devel
BuildRequires:  skia-devel
BuildRequires:  ninja-build
BuildRequires:  make

Requires:       libsqlite3 >= 3.30
Requires:       libX11
Requires:       libwayland-client
Requires:       mesa-libGL

%description
DropAndDrag is a lightweight, cross-platform drag-and-drop shelf that provides
quick access to frequently used files, folders, images, text snippets, and URLs.

Features:
 - Always-on-top floating shelf
 - Drag-and-drop support for files, text, URLs, and images
 - Full-text search with prefix matching
 - Collections and tags for organization
 - Global hotkey to show/hide
 - System tray integration
 - GPU-accelerated rendering via Skia

%prep
%setup -q

%build
%cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_STANDARD=23 \
    -G Ninja
%cmake_build

%install
%cmake_install
mkdir -p %{buildroot}%{_datadir}/applications
cat > %{buildroot}%{_datadir}/applications/dropanddrag.desktop << EOF
[Desktop Entry]
Type=Application
Name=DropAndDrag
Comment=Fast cross-platform drag-and-drop shelf utility
Exec=dropanddrag
Icon=dropanddrag
Categories=Utility;Office;
Terminal=false
StartupNotify=true
EOF

mkdir -p %{buildroot}%{_datadir}/icons/hicolor/256x256/apps
cp %{_builddir}/resources/icons/icon.svg \
    %{buildroot}%{_datadir}/icons/hicolor/256x256/apps/dropanddrag.svg 2>/dev/null || true

%files
%license LICENSE
%{_bindir}/dropanddrag
%{_datadir}/applications/dropanddrag.desktop
%{_datadir}/icons/hicolor/256x256/apps/dropanddrag.svg

%changelog
* Wed Jun 11 2025 DropAndDrag Team <support@dropanddrag.app> - 1.0.0-1
- Initial RPM release
