//
// Created by kat on 5/23/23.
//

#ifndef KSUITE_LOADEDIMAGE_H
#define KSUITE_LOADEDIMAGE_H

#include <binaryninjaapi.h>
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"
#include "libkbinja/MetadataSerializable.hpp"

const std::string SharedCacheMetadataTag = "KSUITE-SharedCacheData";

struct LoadedImage : public MetadataSerializable {
    std::string name;
    uint64_t headerBase;
    std::vector<std::pair<uint64_t, std::pair<uint64_t, uint64_t>>> loadedSegments;
    std::vector<std::pair<std::string, std::pair<uint64_t, uint64_t>>> loadedSections;

    void Store() override {
        MSS(name);
        MSS(headerBase);
        MSS(loadedSegments);
    }
    void Load() override {
        MSL(name);
        MSL(headerBase);
        MSL(loadedSegments);
    }
};
#endif //KSUITE_LOADEDIMAGE_H
