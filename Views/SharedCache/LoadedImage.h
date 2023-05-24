//
// Created by kat on 5/23/23.
//

#ifndef KSUITE_LOADEDIMAGE_H
#define KSUITE_LOADEDIMAGE_H

#include <binaryninjaapi.h>
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"
const std::string SharedCacheMetadataTag = "KSUITE-SharedCacheData";

struct LoadedImage {
    std::string name;
    uint64_t headerBase;
    std::vector<std::pair<uint64_t, std::pair<uint64_t, uint64_t>>> loadedSegments;
    std::vector<std::pair<std::string, std::pair<uint64_t, uint64_t>>> loadedSections;

    rapidjson::Document serialize(rapidjson::Document::AllocatorType& allocator)
    {

        rapidjson::Document d;
        d.SetObject();

        rapidjson::Value loadedSegs(rapidjson::kArrayType);
        for (auto seg : loadedSegments)
        {
            rapidjson::Value segV(rapidjson::kArrayType);
            segV.PushBack(seg.first, allocator);
            segV.PushBack(seg.second.first, allocator);
            segV.PushBack(seg.second.second, allocator);
            loadedSegs.PushBack(segV, allocator);
        }

        d.AddMember("name",  name, allocator);
        d.AddMember("headerBase",   headerBase, allocator);
        d.AddMember("loadedSegments",    loadedSegs, allocator);

        return d;
    }
    static LoadedImage deserialize(rapidjson::Value doc)
    {
        LoadedImage img;
        img.name = doc["name"].GetString();
        img.headerBase = doc["headerBase"].GetUint64();
        for (auto& seg : doc["loadedSegments"].GetArray())
        {
            std::pair<uint64_t, std::pair<uint64_t, uint64_t>> lSeg;
            lSeg.first = seg.GetArray()[0].GetUint64();
            lSeg.second.first = seg.GetArray()[1].GetUint64();
            lSeg.second.second = seg.GetArray()[2].GetUint64();
            img.loadedSegments.push_back(lSeg);
        }
        return img;
    }
};
#endif //KSUITE_LOADEDIMAGE_H
