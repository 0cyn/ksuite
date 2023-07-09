//
// Created by vr1s on 12/17/22.
//

#include <binaryninjaapi.h>
#include "Plugin.h"
#include "XNU/Workflows/DarwinKernel.h"

#ifdef UI_BUILD
#include "uitypes.h"
#include "UI/Notifications.h"
#include "UI/theme/Flattery.h"
#include "UI/Callgraph/Callgraph.h"
#include "Tooling/ExportSegmentAsFile/ExportSegment.h"
#endif

void InitDSCViewType();

extern "C" {

BN_DECLARE_CORE_ABI_VERSION

#ifdef UI_BUILD
BN_DECLARE_UI_ABI_VERSION
#endif

BINARYNINJAPLUGIN bool CorePluginInit() {

    DarwinKernelWorkflow::Register();
    InitDSCViewType();
#ifdef UI_BUILD
    ExportSegment::Register();
    ExportSection::Register();
#endif
    return true;
}
#ifdef UI_BUILD
BINARYNINJAPLUGIN bool UIPluginInit() {

    Notifications::init();
    addJsonTheme(flatteryJson.c_str());
    refreshUserThemes();
    setActiveTheme("Flattery - Dark");

    CallgraphToolInit();

    return true;
}
#endif

}