set(debug_file_SRCS)
ecm_qt_declare_logging_category(
    debug_file_SRCS HEADER plugin_remotekeyboard_debug.h
    IDENTIFIER KDECONNECT_PLUGIN_REMOTEKEYBOARD CATEGORY_NAME kdeconnect.plugin.remotekeyboard
    DEFAULT_SEVERITY Warning
    EXPORT kdeconnect-kde DESCRIPTION "kdeconnect (plugin remotekeyboard)")

kdeconnect_add_plugin(kdeconnect_remotekeyboard
    SOURCES remotekeyboardplugin.cpp ${debug_file_SRCS})

# find_package(Qt5 ${QT_MIN_VERSION} REQUIRED COMPONENTS X11Extras)

target_link_libraries(kdeconnect_remotekeyboard
    kdeconnectcore
    Qt5::Gui
    Qt5::DBus
)
