//
// Created by vr1s on 12/17/22.
//

#include <binaryninjaapi.h>
#include "uitypes.h"
#include "Plugin.h"
#include "DarwinKernelUtils/Workflows/DarwinKernel.h"
#include "UI/Notifications.h"
#include "UI/theme/Flattery.h"
#include "UI/Callgraph/Callgraph.h"
#include "Tooling/ExportSegmentAsFile/ExportSegment.h"

void InitDSCViewType();

extern "C" {

BN_DECLARE_CORE_ABI_VERSION
BN_DECLARE_UI_ABI_VERSION
BINARYNINJAPLUGIN bool CorePluginInit() {

    DarwinKernelWorkflow::Register();
    InitDSCViewType();

    ExportSegment::Register();
    ExportSection::Register();

    return true;
}

BINARYNINJAPLUGIN bool UIPluginInit() {

    Notifications::init();
    addJsonTheme(flatteryJson.c_str());
    refreshUserThemes();
    setActiveTheme("Flattery - Dark");

    CallgraphToolInit();

    return true;
}

}