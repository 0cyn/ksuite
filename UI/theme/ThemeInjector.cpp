//
// Created by vr1s on 5/8/23.
//

#include <binaryninjaapi.h>
#include <dlfcn.h>
#include "ThemeInjector.h"

uint64_t ThemeInjector::Inject()
{
    unsigned char _addThemeJsonPatchfind[32] = "\xff""C\x04\xd1\xfag\x0c\xa9\xf8_\r\xa9\xf6W\x0e\xa9\xf4O\x0f\xa9\xfd{\x10\xa9\xfd\x03\x04\x91\xf6\x03\x00";
    void* binjaUIHandle = dlopen("libbinaryninjaui.1.dylib", RTLD_NOW);
    if (!binjaUIHandle)
        abort();
    auto refreshUserThemesAddress = (uint64_t)dlsym(binjaUIHandle, "_Z17refreshUserThemesv");

    uint64_t maxDistance = 0x10000;
    if (refreshUserThemesAddress)
    {
        for (size_t i = 0; i < maxDistance; i += 4)
        {
            if (memcmp((void*)(refreshUserThemesAddress-i), &_addThemeJsonPatchfind, 28) == 0)
            {
                g_addThemeJsonFunctionPointer = (refreshUserThemesAddress-i);
                goto win;
            }
        }
    }
    goto fail;

win:
    return g_addThemeJsonFunctionPointer;

fail:
    BNLogError("ThemeInjector lookup failed");
    return 0;
}