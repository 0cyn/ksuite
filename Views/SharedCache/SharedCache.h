//
// Created by kat on 5/19/23.
//

#include <binaryninjaapi.h>
#include "LoadedImage.h"
#include "DSCView.h"
#include "VM.h"
#include "Views/MachO/machoview.h"

#ifndef KSUITE_SHAREDCACHE_H
#define KSUITE_SHAREDCACHE_H

struct __attribute__((packed)) dyld_cache_mapping_info {
    uint64_t    address;
    uint64_t    size;
    uint64_t    fileOffset;
    uint32_t    maxProt;
    uint32_t    initProt;
};

struct __attribute__((packed)) dyld_cache_image_info
{
    uint64_t    address;
    uint64_t    modTime;
    uint64_t    inode;
    uint32_t    pathFileOffset;
    uint32_t    pad;
};


struct __attribute__((packed)) dyld_cache_header
{
    char        magic[16];              // e.g. "dyld_v0    i386"
    uint32_t    mappingOffset;          // file offset to first dyld_cache_mapping_info
    uint32_t    mappingCount;           // number of dyld_cache_mapping_info entries
    uint32_t    imagesOffsetOld;        // UNUSED: moved to imagesOffset to prevent older dsc_extarctors from crashing
    uint32_t    imagesCountOld;         // UNUSED: moved to imagesCount to prevent older dsc_extarctors from crashing
    uint64_t    dyldBaseAddress;        // base address of dyld when cache was built
    uint64_t    codeSignatureOffset;    // file offset of code signature blob
    uint64_t    codeSignatureSize;      // size of code signature blob (zero means to end of file)
    uint64_t    slideInfoOffsetUnused;  // unused.  Used to be file offset of kernel slid info
    uint64_t    slideInfoSizeUnused;    // unused.  Used to be size of kernel slid info
    uint64_t    localSymbolsOffset;     // file offset of where local symbols are stored
    uint64_t    localSymbolsSize;       // size of local symbols information
    uint8_t     uuid[16];               // unique value for each shared cache file
    uint64_t    cacheType;              // 0 for development, 1 for production // Kat: , 2 for iOS 16?
    uint32_t    branchPoolsOffset;      // file offset to table of uint64_t pool addresses
    uint32_t    branchPoolsCount;       // number of uint64_t entries
    uint64_t    accelerateInfoAddr;     // (unslid) address of optimization info
    uint64_t    accelerateInfoSize;     // size of optimization info
    uint64_t    imagesTextOffset;       // file offset to first dyld_cache_image_text_info
    uint64_t    imagesTextCount;        // number of dyld_cache_image_text_info entries
    uint64_t    patchInfoAddr;          // (unslid) address of dyld_cache_patch_info
    uint64_t    patchInfoSize;          // Size of all of the patch information pointed to via the dyld_cache_patch_info
    uint64_t    otherImageGroupAddrUnused;    // unused
    uint64_t    otherImageGroupSizeUnused;    // unused
    uint64_t    progClosuresAddr;       // (unslid) address of list of program launch closures
    uint64_t    progClosuresSize;       // size of list of program launch closures
    uint64_t    progClosuresTrieAddr;   // (unslid) address of trie of indexes into program launch closures
    uint64_t    progClosuresTrieSize;   // size of trie of indexes into program launch closures
    uint32_t    platform;               // platform number (macOS=1, etc)
    uint32_t    formatVersion          : 8,  // dyld3::closure::kFormatVersion
    dylibsExpectedOnDisk   : 1,  // dyld should expect the dylib exists on disk and to compare inode/mtime to see if cache is valid
    simulator              : 1,  // for simulator of specified platform
    locallyBuiltCache      : 1,  // 0 for B&I built cache, 1 for locally built cache
    builtFromChainedFixups : 1,  // some dylib in cache was built using chained fixups, so patch tables must be used for overrides
    padding                : 20; // TBD
    uint64_t    sharedRegionStart;      // base load address of cache if not slid
    uint64_t    sharedRegionSize;       // overall size required to map the cache and all subCaches, if any
    uint64_t    maxSlide;               // runtime slide of cache can be between zero and this value
    uint64_t    dylibsImageArrayAddr;   // (unslid) address of ImageArray for dylibs in this cache
    uint64_t    dylibsImageArraySize;   // size of ImageArray for dylibs in this cache
    uint64_t    dylibsTrieAddr;         // (unslid) address of trie of indexes of all cached dylibs
    uint64_t    dylibsTrieSize;         // size of trie of cached dylib paths
    uint64_t    otherImageArrayAddr;    // (unslid) address of ImageArray for dylibs and bundles with dlopen closures
    uint64_t    otherImageArraySize;    // size of ImageArray for dylibs and bundles with dlopen closures
    uint64_t    otherTrieAddr;          // (unslid) address of trie of indexes of all dylibs and bundles with dlopen closures
    uint64_t    otherTrieSize;          // size of trie of dylibs and bundles with dlopen closures
    uint32_t    mappingWithSlideOffset; // file offset to first dyld_cache_mapping_and_slide_info
    uint32_t    mappingWithSlideCount;  // number of dyld_cache_mapping_and_slide_info entries
    uint64_t    dylibsPBLStateArrayAddrUnused;    // unused
    uint64_t    dylibsPBLSetAddr;           // (unslid) address of PrebuiltLoaderSet of all cached dylibs
    uint64_t    programsPBLSetPoolAddr;     // (unslid) address of pool of PrebuiltLoaderSet for each program
    uint64_t    programsPBLSetPoolSize;     // size of pool of PrebuiltLoaderSet for each program
    uint64_t    programTrieAddr;            // (unslid) address of trie mapping program path to PrebuiltLoaderSet
    uint32_t    programTrieSize;
    uint32_t    osVersion;                  // OS Version of dylibs in this cache for the main platform
    uint32_t    altPlatform;                // e.g. iOSMac on macOS
    uint32_t    altOsVersion;               // e.g. 14.0 for iOSMac
    uint64_t    swiftOptsOffset;        // file offset to Swift optimizations header
    uint64_t    swiftOptsSize;          // size of Swift optimizations header
    uint32_t    subCacheArrayOffset;    // file offset to first dyld_subcache_entry
    uint32_t    subCacheArrayCount;     // number of subCache entries
    uint8_t     symbolFileUUID[16];     // unique value for the shared cache file containing unmapped local symbols
    uint64_t    rosettaReadOnlyAddr;    // (unslid) address of the start of where Rosetta can add read-only/executable data
    uint64_t    rosettaReadOnlySize;    // maximum size of the Rosetta read-only/executable region
    uint64_t    rosettaReadWriteAddr;   // (unslid) address of the start of where Rosetta can add read-write data
    uint64_t    rosettaReadWriteSize;   // maximum size of the Rosetta read-write region
    uint32_t    imagesOffset;           // file offset to first dyld_cache_image_info
    uint32_t    imagesCount;            // number of dyld_cache_image_info entries
};

struct __attribute__((packed)) dyld_subcache_entry {
    char uuid[16];
    uint64_t address;
};

struct __attribute__((packed)) dyld_subcache_entry2 {
    char uuid[16];
    uint64_t address;
    char fileExtension[32];
};

using namespace BinaryNinja;
struct KMachOHeader {
    uint64_t textBase = 0;
    uint64_t loadCommandOffset = 0;
    mach_header_64 ident;
    std::string identifierPrefix;

    std::vector<std::pair<uint64_t, bool>> entryPoints;
    std::vector<uint64_t> m_entryPoints; //list of entrypoints

    symtab_command symtab;
    dysymtab_command dysymtab;
    dyld_info_command dyldInfo;
    routines_command_64 routines64;
    function_starts_command functionStarts;
    std::vector<section_64> moduleInitSections;
    linkedit_data_command exportTrie;
    linkedit_data_command chainedFixups {};

    DataBuffer* stringList;
    size_t stringListSize;

    uint64_t relocationBase;
    // Section and program headers, internally use 64-bit form as it is a superset of 32-bit
    std::vector<segment_command_64> segments; //only three types of sections __TEXT, __DATA, __IMPORT
    segment_command_64 linkeditSegment;
    std::vector<section_64> sections;
    std::vector<std::string> sectionNames;

    std::vector<section_64> symbolStubSections;
    std::vector<section_64> symbolPointerSections;

    std::vector<std::string> dylibs;

    build_version_command buildVersion;
    std::vector<build_tool_version> buildToolVersions;

    bool dysymPresent = false;
    bool dyldInfoPresent = false;
    bool exportTriePresent = false;
    bool chainedFixupsPresent = false;
    bool routinesPresent = false;
    bool functionStartsPresent = false;
    bool relocatable = false;
};



class ScopedVMMapSession;

class SharedCache : public MetadataSerializable
{
    friend ScopedVMMapSession;
    /* VIEW STATE BEGIN -- SERIALIZE ALL OF THIS AND STORE IT IN RAW VIEW */

    uint64_t m_rawViewCursor = 0;

    enum ViewState : uint8_t {
        Unloaded,
        Loaded,
        LoadedWithImages,
    } m_viewState;

    std::map<std::string, LoadedImage> m_loadedImages;

    /* VIEWSTATE END */

    /* API VIEW START */
    BinaryNinja::Ref<BinaryNinja::BinaryView> m_dscView;
    /* API VIEW END */

    /* VM READER START */
    bool m_pagesMapped;
    std::shared_ptr<MMappedFileAccessor> m_baseFile;
public:
    std::shared_ptr<VM> m_vm;

    void Store() override {
        MSS(m_viewState);
        MSS(m_rawViewCursor);
        rapidjson::Value loadedImages(rapidjson::kArrayType);
        for (auto img : m_loadedImages)
        {
            loadedImages.PushBack(img.second.AsDocument(), m_activeContext->allocator);
        }
        m_activeContext->doc.AddMember("loadedImages", loadedImages, m_activeContext->allocator);

    }
    ViewState loadViewState(std::string x)
    {
        uint8_t val;
        Deserialize(x, val);
        return (ViewState)val;
    }
    void Load() override {
        m_viewState = loadViewState("m_viewState");
        MSL(m_rawViewCursor);
        for (auto &imgV: m_activeDeserContext->doc["loadedImages"].GetArray())
        {
            if (imgV.HasMember("name"))
            {
                auto name = imgV.FindMember("name");
                if (name != imgV.MemberEnd())
                {
                    LoadedImage img;
                    img.LoadFromValue(imgV);
                    m_loadedImages[name->value.GetString()] = img;
                }
            }
        }
    }

private:
    std::shared_ptr<VMReader> m_vmReader;
    bool SetupVMMap(bool mapPages = true);
    bool TeardownVMMap();
    /* VM READER END */

    /* CACHE FORMAT START */
    enum SharedCacheFormat {
        RegularCacheFormat,
        SplitCacheFormat,
        LargeCacheFormat,
        iOS16CacheFormat,
    };
    SharedCacheFormat GetCacheFormat();
    /* CACHE FORMAT END */

    std::string Serialize();
    void DeserializeFromRawView();

public:
    static SharedCache* GetFromDSCView(BinaryNinja::Ref<BinaryNinja::BinaryView> dscView);
    bool SaveToDSCView()
    {
        if (m_dscView)
        {
            auto data = AsMetadata();
            m_dscView->StoreMetadata(SharedCacheMetadataTag, data);
            m_dscView->GetParentView()->GetParentView()->StoreMetadata(SharedCacheMetadataTag, data);
            BNLogInfo("meta: %s", m_dscView->GetStringMetadata(SharedCacheMetadataTag).c_str());
            return true;
        }
        return false;
    }

    uint64_t GetImageStart(std::string installName);
    bool LoadImageWithInstallName(std::string installName);
    bool LoadSectionAtAddress(uint64_t address);
    std::vector<std::string> GetAvailableImages();

    std::vector<LoadedImage> LoadedImages() const {
        std::vector<LoadedImage> imgs;
        for (const auto& [k, v] : m_loadedImages)
            imgs.push_back(v);
        return imgs;
    }

    explicit SharedCache(BinaryNinja::Ref<BinaryNinja::BinaryView> rawView);
};
class MachOLoader {

public:
    static KMachOHeader HeaderForAddress(Ref<BinaryView> data, uint64_t address, std::string identifierPrefix);
    static void InitializeHeader(Ref<BinaryView> view, KMachOHeader header, uint64_t loadOnlySectionWithAddress = 0);
    static void ParseExportTrie(MMappedFileAccessor* linkeditFile, Ref<BinaryView> view, KMachOHeader header);
};

class ScopedVMMapSession
{
public:
    ScopedVMMapSession(
            SharedCache* cache, bool mapPages = true) :
            m_cache(cache)
    {
        m_cache->SetupVMMap(mapPages);
    };
    ~ScopedVMMapSession()
    {
        m_cache->TeardownVMMap();
    }

private:
    SharedCache* m_cache;
};

void InitDSCViewType();

#endif //KSUITE_SHAREDCACHE_H
