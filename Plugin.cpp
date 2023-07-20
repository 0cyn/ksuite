//
// Created by vr1s on 12/17/22.
//

#include <binaryninjaapi.h>
#include "Plugin.h"
#include "XNU/Workflows/DarwinKernel.h"

#ifdef UI_BUILD
#include "binaryninja-api/ui/uitypes.h"
#include "UI/Notifications.h"
#include "UI/Theme/Flattery.h"
#include "UI/Theme/ThemeEditor.h"
#include "UI/Callgraph/Callgraph.h"
#include "Tooling/ExportSegmentAsFile/ExportSegment.h"
#ifdef NOTEPAD_BUILD
#include "Notepad/NotepadUI.h"
#endif
#endif

#ifdef BUILD_SHAREDCACHE
void InitDSCViewType();
#endif

extern "C" {

BN_DECLARE_CORE_ABI_VERSION

#ifdef UI_BUILD
BN_DECLARE_UI_ABI_VERSION
#endif

BINARYNINJAPLUGIN bool CorePluginInit() {

    DarwinKernelWorkflow::Register();
#ifdef BUILD_SHAREDCACHE
    InitDSCViewType();
#endif
#ifdef UI_BUILD
    ExportSegment::Register();
    ExportSection::Register();
#endif
    return true;
}
#ifdef UI_BUILD
BINARYNINJAPLUGIN bool UIPluginInit() {

    Notifications::init();
#ifdef NOTEPAD_BUILD
    // TODO: consolidate these two classes
    NotepadNotifications::init();
#endif
#ifdef THEME_BUILD
	ThemeEditor editor;
    addJsonTheme(editor.GenerateThemeText().c_str());
    refreshUserThemes();
    setActiveTheme("KSUITE-INTERNALTHEME");
#endif

    CallgraphToolInit();

    return true;
}
#endif

}