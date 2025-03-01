add_executable(fcitx5-wayland-binding-generator main.cpp protocol.cpp parser.cpp)
target_link_libraries(fcitx5-wayland-binding-generator Qt6::Core Qt6::Xml)
