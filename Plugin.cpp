//
// Created by vr1s on 12/17/22.
//

#include <binaryninjaapi.h>
#include "uitypes.h"
#include "Plugin.h"
#include "Workflows/ShutUpAboutPAC.h"
#include "UI/Notifications.h"
#include "UI/theme/ThemeInjector.h"
#include "UI/theme/Flattery.h"

void InitDSCViewType();

extern "C" {

BN_DECLARE_CORE_ABI_VERSION
BN_DECLARE_UI_ABI_VERSION
BINARYNINJAPLUGIN bool CorePluginInit() {

    ShutUpAboutPAC::Register();
    InitDSCViewType();

    return true;
}

BINARYNINJAPLUGIN bool UIPluginInit() {

    Notifications::init();
    g_addThemeJsonFunctionPointer = ThemeInjector::Inject();
    typedef void (* fnPtr)(const char*);
    fnPtr ptr = (fnPtr) g_addThemeJsonFunctionPointer;
    ptr(flatteryJson.c_str());
    refreshUserThemes();
    setActiveTheme("Flattery - Dark");

    return true;
}

}