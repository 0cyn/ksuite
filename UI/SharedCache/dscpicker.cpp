//
// Created by kat on 5/22/23.
//

#include "dscpicker.h"
#include <ksuiteapi.h>

using namespace BinaryNinja;

std::string DisplayDSCPicker(UIContext* ctx, Ref<BinaryView> dscView)
{
    // In this example, we display a list of all defined types to the user and allow them to pick one.
    QStringList entries;
    KAPI::SharedCache* kache = new KAPI::SharedCache(dscView);

    for (auto img : kache->GetAvailableImages())
    {
        entries.push_back(QString::fromStdString(img));
    }

    auto choiceDialog = new MetadataChoiceDialog(ctx->mainWindow(), "Pick Image", entries);
    choiceDialog->AddWidthRequiredByItem(ctx, 300);
    choiceDialog->exec();

    if (choiceDialog->GetChosenEntry().has_value())
        return entries.at(choiceDialog->GetChosenEntry().value().idx).toStdString();
    else
        return {};
}
