//
// Created by kat on 5/19/23.
//

#include <binaryninjaapi.h>
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"

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


class MissingFileException : public std::exception
{
    virtual const char* what() const throw()
    {
        return "Missing File.";
    }
};



class MemoryException : public std::exception {
    virtual const char *what() const throw() {
        return "Memory Limit Reached";
    }
};


struct MMAP {
    void *_mmap;
    FILE *fd;
    size_t len;

    bool mapped;

    void Map();

    void Unmap();
};


class MMappedFileAccessor {
    std::string m_path;
    MMAP m_mmap;

public:

    MMappedFileAccessor(std::string &path);

    ~MMappedFileAccessor();

    std::string Path() const { return m_path; };

    size_t Length() const { return m_mmap.len; };

    void *Data() const { return m_mmap._mmap; };

    std::string ReadNullTermString(size_t address);

    uint8_t ReadUChar(size_t address);

    int8_t ReadChar(size_t address);

    uint16_t ReadUShort(size_t address);

    int16_t ReadShort(size_t address);

    uint32_t ReadUInt32(size_t address);

    int32_t ReadInt32(size_t address);

    uint64_t ReadULong(size_t address);

    int64_t ReadLong(size_t address);

    BinaryNinja::DataBuffer *ReadBuffer(size_t addr, size_t length);

    void Read(void *dest, size_t addr, size_t length);
};


struct PageMapping {
    std::shared_ptr<MMappedFileAccessor> file;
    size_t fileOffset;
    size_t pagesRemaining;
};


class VMException : public std::exception {
    virtual const char *what() const throw() {
        return "Generic VM Exception";
    }
};

class MappingPageAlignmentException : public VMException {
    virtual const char *what() const throw() {
        return "Tried to create a mapping not aligned to given page size";
    }
};

class MappingReadException : VMException {
    virtual const char *what() const throw() {
        return "Tried to access unmapped page";
    }
};

class MappingCollisionException : VMException {
    virtual const char *what() const throw() {
        return "Tried to remap a page";
    }
};

class VMReader;


class VM {
    std::map<size_t, PageMapping> m_map;
    size_t m_pageSize;
    size_t m_pageSizeBits;
    bool m_safe;

    friend VMReader;

public:

    VM(size_t pageSize, bool safe = true);

    ~VM();

    void MapPages(size_t vm_address, size_t fileoff, size_t size, std::shared_ptr<MMappedFileAccessor> file);

    bool AddressIsMapped(uint64_t address);

    std::pair<PageMapping, size_t> MappingAtAddress(size_t address);

    std::string ReadNullTermString(size_t address);

    uint8_t ReadUChar(size_t address);

    int8_t ReadChar(size_t address);

    uint16_t ReadUShort(size_t address);

    int16_t ReadShort(size_t address);

    uint32_t ReadUInt32(size_t address);

    int32_t ReadInt32(size_t address);

    uint64_t ReadULong(size_t address);

    int64_t ReadLong(size_t address);

    BinaryNinja::DataBuffer *ReadBuffer(size_t addr, size_t length);

    void Read(void *dest, size_t addr, size_t length);
};


class VMReader {
    std::shared_ptr<VM> m_vm;
    size_t m_cursor;
    size_t m_addressSize;

public:
    VMReader(std::shared_ptr<VM> vm, size_t addressSize = 8);

    void Seek(size_t address);

    void SeekRelative(size_t offset);

    [[nodiscard]] size_t Offset() const { return m_cursor; }

    std::string ReadNullTermString(size_t address);

    uint64_t ReadULEB128(size_t cursorLimit);

    int64_t ReadSLEB128(size_t cursorLimit);

    uint8_t ReadUChar();

    int8_t ReadChar();

    uint16_t ReadUShort();

    int16_t ReadShort();

    uint32_t ReadUInt32();

    int32_t ReadInt32();

    uint64_t ReadULong();

    int64_t ReadLong();

    size_t ReadPointer();

    uint8_t ReadUChar(size_t address);

    int8_t ReadChar(size_t address);

    uint16_t ReadUShort(size_t address);

    int16_t ReadShort(size_t address);

    uint32_t ReadUInt32(size_t address);

    int32_t ReadInt32(size_t address);

    uint64_t ReadULong(size_t address);

    int64_t ReadLong(size_t address);

    size_t ReadPointer(size_t address);

    BinaryNinja::DataBuffer *ReadBuffer(size_t length);

    BinaryNinja::DataBuffer *ReadBuffer(size_t addr, size_t length);

    void Read(void *dest, size_t length);

    void Read(void *dest, size_t addr, size_t length);
};


class DSCRawView : public BinaryNinja::BinaryView {
    std::string m_filename;
public:

    DSCRawView(const std::string &typeName, BinaryView *data, bool parseOnly = false);

    bool Init() override;
};


class DSCRawViewType : public BinaryNinja::BinaryViewType {

public:
    BinaryNinja::Ref<BinaryNinja::BinaryView> Create(BinaryNinja::BinaryView* data) override;
    BinaryNinja::Ref<BinaryNinja::BinaryView> Parse(BinaryNinja::BinaryView* data) override;
    bool IsTypeValidForData(BinaryNinja::BinaryView *data) override;

    bool IsDeprecated() override { return false; }

    BinaryNinja::Ref<BinaryNinja::Settings> GetLoadSettingsForData(BinaryNinja::BinaryView *data) override { return nullptr; }

public:
    DSCRawViewType();
};


class DSCView : public BinaryNinja::BinaryView {

public:

    DSCView(const std::string &typeName, BinaryView *data, bool parseOnly = false);

    bool Init() override;
};


class DSCViewType : public BinaryNinja::BinaryViewType {

public:
    DSCViewType();

    BinaryNinja::Ref<BinaryNinja::BinaryView> Create(BinaryNinja::BinaryView *data) override;

    BinaryNinja::Ref<BinaryNinja::BinaryView> Parse(BinaryNinja::BinaryView *data) override;

    bool IsTypeValidForData(BinaryNinja::BinaryView *data) override;

    bool IsDeprecated() override { return false; }

    BinaryNinja::Ref<BinaryNinja::Settings> GetLoadSettingsForData(BinaryNinja::BinaryView *data) override { return nullptr; }
};


const std::string SharedCacheMetadataTag = "KSUITE-SharedCacheData";
class ScopedVMMapSession;

class SharedCache
{
    friend ScopedVMMapSession;
    /* VIEW STATE BEGIN -- SERIALIZE ALL OF THIS AND STORE IT IN RAW VIEW */

    uint64_t m_rawViewCursor = 0;

    enum ViewState : uint8_t {
        Unloaded,
        Loaded,
        LoadedWithImages,
    } m_viewState;

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
    std::map<std::string, LoadedImage> m_loadedImages;

    /* VIEWSTATE END */

    /* API VIEW START */
    BinaryNinja::Ref<BinaryNinja::BinaryView> m_rawView;
    /* API VIEW END */

    /* VM READER START */
    bool m_pagesMapped;
    std::shared_ptr<MMappedFileAccessor> m_baseFile;
    std::shared_ptr<VM> m_vm;
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
    static SharedCache* GetFromRawView(BinaryNinja::Ref<BinaryNinja::BinaryView> rawView);
    static SharedCache* GetFromDSCView(BinaryNinja::Ref<BinaryNinja::BinaryView> dscView)
    {
        return GetFromRawView(dscView->GetParentView());
    }
    bool SaveToRawView()
    {
        if (m_rawView)
        {
            BinaryNinja::Ref<BinaryNinja::Metadata> data = new BinaryNinja::Metadata(Serialize());
            m_rawView->StoreMetadata(SharedCacheMetadataTag, data);
            BNLogInfo("meta: %s", m_rawView->GetStringMetadata(SharedCacheMetadataTag).c_str());
            return true;
        }
        return false;
    }

    bool LoadImageWithInstallName(std::string installName);
    std::vector<std::string> GetAvailableImages();

    explicit SharedCache(BinaryNinja::Ref<BinaryNinja::BinaryView> rawView);
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
