//
// Created by kat on 5/22/23.
//

#ifndef KSUITE_DSCPICKER_H
#define KSUITE_DSCPICKER_H

#include <binaryninjaapi.h>
#include <ui/metadatachoicedialog.h>

std::string DisplayDSCPicker(UIContext* ctx, BinaryNinja::Ref<BinaryNinja::BinaryView> dscView);

#endif //KSUITE_DSCPICKER_H
