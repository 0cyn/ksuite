//
// Created by kat on 5/21/23.
//

#ifndef KSUITE_SHARED_H
#define KSUITE_SHARED_H

#include <binaryninjaapi.h>

class SidebarWidgetTypeExtended {
public:
    virtual bool ValidForView(BinaryNinja::BinaryView* view) = 0;
};

#endif //KSUITE_SHARED_H
