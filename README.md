# Arc Window Decoration
Window decoration for KDE Plasma based on the Breeze window decoration.

## Features
* Faithful recreation of the [GTK Arc theme](https://github.com/horst3180/arc-theme) window decoration up to button spacing
* Supports Arc and Arc Dark themes
* Shadows as in Breeze window decoration, including the same customization options
* Option for using [Arc Aurorae Window Decoration](https://github.com/PapirusDevelopmentTeam/arc-kde/) icons
* Window specific theme overrides allowing mixing of light and dark theme
* Window resizing that works by dragging window borders regardless of border size

## Install/Build
This window decoration must be built and installed from source. It has been tested and built against KDE Plasma 5.19.

At least the following packages are required on KDE Neon:

```
build-essential cmake libkf5config-dev libkdecorations2-dev libqt5x11extras5-dev qtdeclarative5-dev extra-cmake-modules libkf5guiaddons-dev libkf5configwidgets-dev libkf5windowsystem-dev libkf5coreaddons-dev
```

The CMake configuration phase will helpfully report any further possible missing packages.

When all required dependencies are installed, the window decoration may be built and installed as follows:

```
mkdir build
cd build
cmake ../src -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release -DKDE_INSTALL_LIBDIR=lib -DBUILD_TESTING=OFF -DKDE_INSTALL_USE_QT_SYS_PATHS=ON
sudo make install
```

After successful installation, you either need to log out or use the command `kwin_x11 --replace &` before the window decoration can be used.

## Recommended Complements
* Plasma Style: Arc Color or Arc Dark from the [Arc KDE](https://github.com/PapirusDevelopmentTeam/arc-kde/) package
* Application Style: [KvArc](https://github.com/tsujan/Kvantum/tree/master/Kvantum/themes/kvthemes) (or [Arc](https://github.com/PapirusDevelopmentTeam/arc-kde/) from Arc KDE) [Kvantum](https://github.com/tsujan/Kvantum/tree/master/Kvantum) themes and their variants
* Colors: Arc or Arc Dark from the [Arc KDE](https://github.com/PapirusDevelopmentTeam/arc-kde/) package
* Icons: [Papirus icon theme](https://github.com/PapirusDevelopmentTeam/papirus-icon-theme)
* GTK Style: [Arc theme](https://github.com/arc-design/arc-theme)

## Acknowledgments
* horst3180 for the [original GTK Arc theme](https://github.com/horst3180/arc-theme)
* varlesh for the [Arc Aurorae window decorations](https://github.com/PapirusDevelopmentTeam/arc-kde/)
* Chris Holland for [tutorial on how create custom window decorations](https://zren.github.io/2017/07/08/patching-breeze-window-decorations)
