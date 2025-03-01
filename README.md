fcitx5-wayland-binding-generator
========================================================
This is a simple C++ binding generator that is designed to be used with Fcitx 5.

The binding relies on proxy object's user data to store the coresponding C++
binding object pointer to work.

Right now, it is not targeting to be up to date, or support all wayland protocol
xml feature. And the binding will only work for client side, not server side.

Build dependency:
cmake
Qt6Core

Build step:
mkdir build
cmake ..
cmake --build .

Usage:
 ./bin/fcitx5-wayland-binding-generator \
     --includes wayland-plasma-window-management-client-protocol.h \
     --namespace fcitx::wayland \
     --directory output/ \
     plasma-window-management.xml
