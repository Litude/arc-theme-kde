cmake_minimum_required(VERSION 3.0)
project(arcdecoration)
set(PROJECT_VERSION "1.00")
set(PROJECT_VERSION_MAJOR 1)

set(KF5_MIN_VERSION "5.66.0")

include(GenerateExportHeader)
include(WriteBasicConfigVersionFile)
include(FeatureSummary)

find_package(ECM ${KF5_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR} ${CMAKE_SOURCE_DIR}/cmake)

include(ECMInstallIcons)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDEClangFormat)

find_package(KDecoration2 REQUIRED)

add_subdirectory(libbreezecommon)

add_definitions(-DTRANSLATION_DOMAIN="breeze_kwin_deco")

find_package(KF5 REQUIRED COMPONENTS CoreAddons GuiAddons ConfigWidgets WindowSystem I18n IconThemes)
find_package(Qt5 CONFIG REQUIRED COMPONENTS DBus)

### XCB
find_package(XCB COMPONENTS XCB)
set_package_properties(XCB PROPERTIES
  DESCRIPTION "X protocol C-language Binding"
  URL "https://xcb.freedesktop.org"
  TYPE OPTIONAL
  PURPOSE "Required to pass style properties to native Windows on X11 Platform"
)

if(UNIX AND NOT APPLE)

  set(BREEZE_HAVE_X11 ${XCB_XCB_FOUND})
  if (XCB_XCB_FOUND)
    find_package(Qt5 REQUIRED CONFIG COMPONENTS X11Extras)
  endif()

else()

  set(BREEZE_HAVE_X11 FALSE)

endif()

################# configuration #################
configure_file(config-arc.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-arc.h )

################# includes #################
include_directories(${CMAKE_SOURCE_DIR}/libbreezecommon)
include_directories(${CMAKE_BINARY_DIR}/libbreezecommon)

################# newt target #################
### plugin classes
set(arcdecoration_SRCS
    arcbutton.cpp
    arcdecoration.cpp
    arcexceptionlist.cpp
    arcsettingsprovider.cpp)

kconfig_add_kcfg_files(arcdecoration_SRCS arcsettings.kcfgc)

### config classes
### they are kept separately because they might move in a separate library in the future
set(arcdecoration_config_SRCS
    config/arcconfigwidget.cpp
    config/arcdetectwidget.cpp
    config/arcexceptiondialog.cpp
    config/arcexceptionlistwidget.cpp
    config/arcexceptionmodel.cpp
    config/arcitemmodel.cpp
)

set(arcdecoration_config_PART_FORMS
   config/ui/arcconfigurationui.ui
   config/ui/arcdetectwidget.ui
   config/ui/arcexceptiondialog.ui
   config/ui/arcexceptionlistwidget.ui
)

ki18n_wrap_ui(arcdecoration_config_PART_FORMS_HEADERS ${arcdecoration_config_PART_FORMS})

### build library
add_library(arcdecoration MODULE
    ${arcdecoration_SRCS}
    ${arcdecoration_config_SRCS}
    ${arcdecoration_config_PART_FORMS_HEADERS})

target_link_libraries(arcdecoration
    PUBLIC
        Qt5::Core
        Qt5::Gui
        Qt5::DBus
    PRIVATE
        breezecommon5
        KDecoration2::KDecoration
        KF5::ConfigCore
        KF5::CoreAddons
        KF5::ConfigWidgets
        KF5::GuiAddons
        KF5::I18n
        KF5::IconThemes
        KF5::WindowSystem)

if(BREEZE_HAVE_X11)
  target_link_libraries(arcdecoration
    PUBLIC
      Qt5::X11Extras
      XCB::XCB)
endif()


install(TARGETS arcdecoration DESTINATION ${PLUGIN_INSTALL_DIR}/org.kde.kdecoration2)
install(FILES config/arcdecorationconfig.desktop DESTINATION ${SERVICES_INSTALL_DIR})
