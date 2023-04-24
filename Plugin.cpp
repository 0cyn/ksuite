//
// Created by Katherine on 12/17/22.
//

#include <binaryninjaapi.h>
#include "uitypes.h"
#include "Plugin.h"
#include "Workflows/ShutUpAboutPAC.h"


extern "C" {

BN_DECLARE_CORE_ABI_VERSION
BN_DECLARE_UI_ABI_VERSION

BINARYNINJAPLUGIN bool CorePluginInit() {

    ShutUpAboutPAC::Register();

    return true;
}

BINARYNINJAPLUGIN bool UIPluginInit() {

    return true;
}

}