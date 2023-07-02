//
// Created by kat on 7/1/23.
//

#ifndef KSUITE_EXPORTSEGMENT_H
#define KSUITE_EXPORTSEGMENT_H

#include "libkbinja/hex.h"
#include <binaryninjaapi.h>
#include "metadatachoicedialog.h"
#include <fstream>

using namespace BinaryNinja;

class ExportSegment
{
public:
    static void Register()
    {
        PluginCommand::Register("Export Segment", "Export Segment as file",
                                [](BinaryView* view)
                                {
                                    ExportSegment::Run(view);
                                }
        );
    }

    static void Run(BinaryView* view)
    {
        std::vector<Ref<Segment>> segments = view->GetSegments();
        QStringList entries;
        for (auto seg : segments)
            entries.push_back("0x" + QString::fromStdString(formatAddress(seg->GetStart())) + " - " + "0x" + QString::fromStdString(formatAddress(seg->GetEnd())));
        auto mdc = new MetadataChoiceDialog(nullptr, "Select Segment", entries);
        mdc->exec();
        if (mdc->GetChosenEntry().has_value())
        {
            Ref<Segment> segment = segments.at(mdc->GetChosenEntry()->idx);
            DataBuffer contents = view->ReadBuffer(segment->GetStart(), segment->GetLength());
            std::string result;
            if (BinaryNinja::GetSaveFileNameInput(result, "Output File"))
            {
                std::ofstream fout;
                fout.open(result, std::ios::binary | std::ios::out);
                fout.write(static_cast<const char *>(contents.GetData()), contents.GetLength());
                fout.close();
            }
        }
    }
};

class ExportSection
{
public:
    static void Register()
    {
        PluginCommand::Register("Export Section", "Export Section as file",
                                [](BinaryView* view)
                                {
                                    ExportSection::Run(view);
                                }
        );
    }

    static void Run(BinaryView* view)
    {
        std::vector<Ref<Section>> sections = view->GetSections();
        QStringList entries;
        for (auto sect : sections)
            entries.push_back(QString::fromStdString(sect->GetName()));
        auto mdc = new MetadataChoiceDialog(nullptr, "Select Section", entries);
        mdc->AddColumn("Address", [&sections](EntryItem item) -> QString {
            QString n;
            Ref<Section> sect = sections.at(item.idx);
            n = QString::fromStdString("0x" + formatAddress(sect->GetStart()) + " - 0x" + formatAddress(sect->GetStart() + sect->GetLength()));
            return n;
        });
        mdc->exec();
        if (mdc->GetChosenEntry().has_value())
        {
            Ref<Section> section = sections.at(mdc->GetChosenEntry()->idx);
            DataBuffer contents = view->ReadBuffer(section->GetStart(), section->GetLength());
            std::string result;
            if (BinaryNinja::GetSaveFileNameInput(result, "Output File"))
            {
                std::ofstream fout;
                fout.open(result, std::ios::binary | std::ios::out);
                fout.write(static_cast<const char *>(contents.GetData()), contents.GetLength());
                fout.close();
            }
        }
    }
};

#endif //KSUITE_EXPORTSEGMENT_H
