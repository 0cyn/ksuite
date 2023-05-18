#pragma once

#include "binaryninjaapi.h"
#include <atomic>
#include <memory>
#include <chrono>
#include <vector>
#include <string>
#include "../MachO/machoview.h"

using namespace BinaryNinja;

constexpr auto dyldRawTypes = R"(
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
};)";

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
    MMappedFileAccessor *file;
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

    void MapPages(size_t vm_address, size_t fileoff, size_t size, MMappedFileAccessor *file);

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
    VM *m_vm;
    size_t m_cursor;
    size_t m_addressSize;

public:
    VMReader(VM *vm, size_t addressSize = 8);

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

static const char *LogPrefix = "sharedcache";

using namespace BinaryNinja;

// rip puppy
class DSCView;

using high_res_clock = std::chrono::high_resolution_clock;


struct MetricDurationItem {
    std::string itemTitle;

    bool started = false;
    bool ended = false;
    high_res_clock::time_point startTime;
    high_res_clock::time_point endTime;

    void StartTimer() {
        started = true;
        ended = false;
        startTime = high_res_clock::now();
    }

    void EndTimer() {
        endTime = high_res_clock::now();
        ended = true;
        started = false;
    }

    template<typename T>
    T Duration() {
        return std::chrono::duration_cast<T>(endTime - startTime);
    }

    template<typename T>
    T DurationSinceLaunch() {
        return std::chrono::duration_cast<T>(high_res_clock::now() - startTime);
    }

};


struct Metrics {
    std::vector<MetricDurationItem *> durationItems;
    std::atomic<int> mappedFiles = 0;

    MetricDurationItem *AddMetric(std::string itemTitle);

    void ClearMetrics() { durationItems.clear(); }

    Metrics() = default;

    ~Metrics() = default;
};


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


struct DynamicHeaderStructure {
    size_t header_size;
    dyld_cache_header header;
};

class SharedCache;

struct ImageSectionRange {
    uint64_t start;
    uint64_t length;
    uint64_t fstart;
    uint64_t flen;
    uint64_t initprot;
};

struct CacheImage {
    std::string installName;
    uint64_t headerAddress;
    std::vector<ImageSectionRange> addressRanges;
};

struct LoadedImageBuffer {
    uint64_t address;
    uint64_t faddr;
    uint64_t initprot;
    BinaryNinja::DataBuffer* data;
};


enum SharedCacheFormat {
    RegularCacheFormat,
    SplitCacheFormat,
    LargeCacheFormat,
    iOS16CacheFormat,
};

struct AddressRange {
    size_t start;
    size_t len;
    size_t end; // precompute this too.
    std::string installName;
};

struct SharedCache {
    SharedCacheFormat format;
    VM* m_vm;
    VM* m_fileVM;
    std::vector<AddressRange> m_mappedRanges;
    std::vector<AddressRange> m_dyldDataSections;
    MMappedFileAccessor* m_baseFile;
    std::vector<std::pair<uint64_t, MMappedFileAccessor*>> m_nonBaseFiles;
    std::vector<std::string> m_installNames;
    std::unordered_map<std::string, std::string> m_baseNames;
    std::map<std::string, CacheImage> m_images;

    std::pair<CacheImage, std::vector<LoadedImageBuffer>> LoadImageByInstallName(std::string& installName);
    std::pair<CacheImage, std::vector<LoadedImageBuffer>> LoadImageByBaseName(std::string& installName);

    static SharedCache* LoadCache(MMappedFileAccessor* file);

private:
    static SharedCacheFormat GetCacheFormat(MMappedFileAccessor* file);
};


class DSCFormatException : public std::exception {
    virtual const char *what() const throw() {
        return "Malformed Shared Cache";
    }
};



namespace CustomTypes {
    const std::string TaggedPointer = "tptr_t";
    const std::string FastPointer = "fptr_t";
    const std::string RelativePointer = "rptr_t";

    const std::string ID = "id";
    const std::string Selector = "SEL";

    const std::string CFString = "CFString";

    const std::string MethodList = "objc_method_list_t";
    const std::string Method = "objc_method_t";
    const std::string MethodListEntry = "objc_method_entry_t";
    const std::string Class = "objc_class_t";
    const std::string ClassRO = "objc_class_ro_t";
}

struct ObjCTypes {
    QualifiedName TaggedPointer;
    QualifiedName FastPointer;
    QualifiedName RelativePointer;

    QualifiedName ID;
    QualifiedName Selector;

    QualifiedName CFString;

    QualifiedName MethodList;
    QualifiedName Method;
    QualifiedName MethodListEntry;
    QualifiedName Class;
    QualifiedName ClassRO;
};


namespace DSCObjC {

    struct ClassRO {
        uint32_t flags;     //0
        uint32_t start;     //4
        uint32_t size;      //8
        uint32_t r;         //12
        uint64_t ivar_layout;//16
        uint64_t name;      //24
        uint64_t methods;   //32
        uint64_t protocols;
        uint64_t ivars;
        uint64_t weak_ivar_layout;
        uint64_t properties;
    };

    struct Class {
        uint64_t isa;
        uint64_t super;
        uint64_t cache;
        uint64_t vtable;
        ClassRO *ro_data;
    };

    struct Method {
        uint64_t name;
        uint64_t types;
        uint64_t imp;
    };
}


class MinLoader {
public:
    static std::vector<ImageSectionRange> SectionsInHeader(VMReader *reader);
    static std::pair<linkedit_data_command, uint64_t> ExportTrieAndLinkeditBaseForImage(VMReader* reader);
    static std::pair<symtab_command, segment_command_64> SymtabAndLinkeditBaseForImage(VMReader* reader);
};


enum BindingOpcodes {
    DONE = 0x0,
    SET_DYLIB_ORDINAL_IMM = 0x10,
    SET_DYLIB_ORDINAL_ULEB = 0x20,
    SET_DYLIB_SPECIAL_IMM = 0x30,
    SET_SYMBOL_TRAILING_FLAGS_IMM = 0x40,
    SET_TYPE_IMM = 0x50,
    SET_ADDEND_SLEB = 0x60,
    SET_SEGMENT_AND_OFFSET_ULEB = 0x70,
    ADD_ADDR_ULEB = 0x80,
    DO_BIND = 0x90,
    DO_BIND_ADD_ADDR_ULEB = 0xa0,
    DO_BIND_ADD_ADDR_IMM_SCALED = 0xb0,
    DO_BIND_ULEB_TIMES_SKIPPING_ULEB = 0xc0,
    THREADED = 0xd0,
};

enum BindingStuff {
    BIND_SUBOPCODE_THREADED_SET_BIND_ORDINAL_TABLE_SIZE_ULEB = 0x00,
    BIND_SUBOPCODE_THREADED_APPLY = 0x01
};

struct BindingRecord {
    size_t off;
    size_t seg_index;
    size_t seg_offset;
    size_t lib_ordinal;
    size_t type;
    size_t flags;
    std::string name;
    size_t addend;
    size_t special_dylib;
};

struct MachOTypes {
    QualifiedName CPUTypeEnum;
    QualifiedName FileTypeEnum;
    QualifiedName FlagsTypeEnum;
    QualifiedName HeaderType;
    QualifiedName Header32Type;
    QualifiedName CmdTypeEnum;
    QualifiedName LoadCommand;
    QualifiedName ProtTypeEnum;
    QualifiedName SegFlagsTypeEnum;
    QualifiedName LoadSegmentCommandType;
    QualifiedName LoadSegmentCommand64Type;
    QualifiedName SectionType;
    QualifiedName Section64Type;
    QualifiedName SymTabType;
    QualifiedName DynSymTabType;
    QualifiedName UUIDType;
    QualifiedName LinkEditData;
    QualifiedName EncryptionInfoType;
    QualifiedName VersionMinType;
    QualifiedName DyldInfoType;
    QualifiedName DylibType;
    QualifiedName DylibCommandType;
};

struct MachOImage {
    uint64_t baseAddress;
    bool is64;
    std::string installName;
    std::vector<segment_command_64> m_segments;
    std::unordered_map<std::string, section_64> sectionsByName;
    std::vector<section_64> m_sections;
    std::vector<std::string> m_deps;

    bool hasObjCMetadata;

    segment_command_64 m_linkeditSegment{};

    dyld_info_command dyldInfo{};
    symtab_command symtabCmd{};
    linkedit_data_command exportTrie{}; // if this exists, ignore dyldInfo export trie TODO validate that's accurate?
};

struct ExportedNode {
    size_t offset;
    size_t flags;
    std::string text;
};


class DSCView;
class SharedCache;

/**
 * Parser for Objective-C type strings.
 */
class ObjCTypeParser {
public:
    /**
     * Parse an encoded type string.
     */
    static std::vector<std::string> parseEncodedType(const std::string &);
};


class ObjCProcessing {
    SharedCache *m_cache;
    DSCView *m_view;
    ObjCTypes m_types{};
    Ref<Logger> m_logger;

    std::mutex m_typeDefMutex;
    bool m_typesLoaded;

    std::optional<uint64_t> m_customRelativeMethodSelectorBase;

    void LoadTypes();

    std::vector<DSCObjC::Class *> GetClassList(MachOImage &image);

    std::vector<DSCObjC::Method> LoadMethodList(uint64_t addr);

    void ApplyMethodType(std::string className, std::string sel, std::string types, uint64_t imp);

public:
    ObjCProcessing(DSCView *view, SharedCache *cache);

    /*!
     * This attempts to replicate (some of) the processing done by workflow_objc
     *
     * Specifically, it handles anything that can be done pre-analysis.
     *
     * @param image MachOImage returned from MachOProcessor
     */
    void LoadObjCMetadata(MachOImage &image);
};

class DSCFileView : public BinaryView {

public:
    std::string m_filename;
    SharedCache *m_cache;
    uint64_t m_length;

    DSCFileView(const std::string &typeName, BinaryView *data, bool parseOnly = false);

protected:
    size_t PerformRead(void *dest, uint64_t offset, size_t len) override;

    bool PerformIsOffsetBackedByFile(uint64_t offset) override;

    uint64_t PerformGetLength() const override;
};


class DSCViewMetadataHandler {
    std::vector<uint64_t> m_loadedImageVirtualBases;
};


class DSCView : public BinaryView {

    friend MachOTypes;

    Metrics *m_metrics;
    Ref<Logger> m_logger;

    ObjCProcessing *m_objcProcessor;

    DSCFileView *m_dscFileView;

    // bv stuff
    bool m_parseOnly;
    Ref<Architecture> m_arch;
    size_t m_addressSize;

    bool m_warmLoad;

    size_t m_pointerMask;

    std::unordered_map<std::string, MachOImage> m_loadedImages;
    std::vector<AddressRange> m_loadedAddressRanges;

    std::vector<AddressRange> m_addressRanges;

    std::unordered_map<uint64_t, std::string> m_symbolCache;

    std::mutex m_typeDefMutex;
    bool m_typesLoaded;
    bool m_objcTypesLoaded;

    MachOTypes m_types{};
    std::string m_filename;
public:
    SharedCache *m_cache;

protected:
    bool PerformIsExecutable() const override { return true; }

public:
    DSCView(const std::string &typeName, DSCFileView *data, bool parseOnly = false);
    ~DSCView();

    bool Init() override;

    static DSCView* FetchDSCViewFromBinaryViewAPI(const Ref<BinaryView>& view);

    std::vector<std::string> GetImageList() { return m_cache->m_installNames; };

    VMReader *GetReader() { return new VMReader(m_cache->m_vm); };

    Ref<Logger> IssueSessionLogger(std::string component);

    Metrics *GetMetrics() { return m_metrics; };

    void ProcessSlideInfo();

    // MachO Items

    void LoadImageViaInstallName(std::string installName, bool isBaseName = false);

    void LoadImageContainingAddress(uint64_t address);

    void LoadStubsSection(uint64_t start, uint64_t end);

    void LoadMachoTypes();

    Ref<Symbol> DefineMachoSymbol(BNSymbolType type, const std::string &name, uint64_t addr, BNSymbolBinding binding,
                                  bool deferred);

    void LoadSymtab(MachOImage &image);

    void LoadDepExports(uint64_t imageBase, linkedit_data_command cmd, uint64_t linkeditBase);

    void LoadExportTrie(MachOImage &image);

    void ParseBindingTable(MachOImage &image, size_t table_start, size_t tableSize);

    std::vector<ExportTrieEntryStart> ReadExportNode(DataBuffer& buffer, std::vector<ExportNode>& results, const std::string& currentText, size_t cursor, uint32_t endGuard);

    MachOImage ProcessImageAtAddress(std::string installName, size_t address);
};

class DSCViewType : public BinaryViewType {

public:
    DSCViewType();

    Ref<BinaryView> Create(BinaryView *data) override;

    Ref<BinaryView> Parse(BinaryView *data) override;

    bool IsTypeValidForData(BinaryView *data) override;

    bool IsDeprecated() override { return false; }

    Ref<Settings> GetLoadSettingsForData(BinaryView *data) override { return nullptr; }
};

class DSCRawView : public BinaryView
{
public:
    DSCRawView(BinaryView* data, bool parseOnly = false);
    virtual ~DSCRawView() {};

    virtual bool Init() override;
    virtual BNEndianness PerformGetDefaultEndianness() const override { return BigEndian; }
    virtual bool PerformIsExecutable() const override { return false; }
    virtual bool PerformIsRelocatable() const override { return false; };
};

class DSCRawViewType : public BinaryViewType {

public:
    Ref<BinaryView> Create(BinaryView* data) override;
    Ref<BinaryView> Parse(BinaryView* data) override;
    bool IsTypeValidForData(BinaryView *data) override;

    bool IsDeprecated() override { return false; }

    Ref<Settings> GetLoadSettingsForData(BinaryView *data) override { return nullptr; }

public:
    DSCRawViewType();
};

void InitDSCViewType();

static DSCView* g_dscViewInstance;


