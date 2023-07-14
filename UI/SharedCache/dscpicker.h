//
// Created by kat on 5/22/23.
//

#ifndef KSUITE_DSCPICKER_H
#define KSUITE_DSCPICKER_H

#include <binaryninjaapi.h>
#include <ui/metadatachoicedialog.h>
#ifdef BUILD_SHAREDCACHE
std::string DisplayDSCPicker(UIContext* ctx, BinaryNinja::Ref<BinaryNinja::BinaryView> dscView);
#endif
#endif //KSUITE_DSCPICKER_H
