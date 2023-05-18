//
// Created by kat on 2/11/23.
//

#define EXPORT_TRIE_DEBUG FALSE
#define DEPENDENCY_DEBUG TRUE
#define BINDING_DEBUG FALSE

#include "SharedCache.h"
#include "highlevelilinstruction.h"
#include <filesystem>
#include <utility>
#include <csignal>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <memory>



static int64_t readSLEB128(DataBuffer& buffer, size_t length, size_t &offset)
{
    uint8_t cur;
    int64_t value = 0;
    size_t shift = 0;
    while (offset < length)
    {
        cur = buffer[offset++];
        value |= (cur & 0x7f) << shift;
        shift += 7;
        if ((cur & 0x80) == 0)
            break;
    }
    value = (value << (64 - shift)) >> (64 - shift);
    return value;
}


static uint64_t readLEB128(DataBuffer& p, size_t end, size_t &offset)
{
    uint64_t result = 0;
    int bit = 0;
    do {
        if (offset >= end)
            return -1;

        uint64_t slice = p[offset] & 0x7f;

        if (bit > 63)
            return -1;
        else {
            result |= (slice << bit);
            bit += 7;
        }
    } while (p[offset++] & 0x80);
    return result;
}


uint64_t readValidULEB128(DataBuffer& buffer, size_t& cursor)
{
    uint64_t value = readLEB128(buffer, buffer.GetLength(), cursor);
    if ((int64_t)value == -1)
        throw ReadException();
    return value;
}


std::unordered_map<size_t, Ref<DSCView>> g_dscViews;


void MMAP::Map() {
    fseek(fd, 0L, SEEK_END);
    len = ftell(fd);

    _mmap = mmap(nullptr, len, PROT_READ | PROT_WRITE, MAP_PRIVATE, fileno(fd), 0u);
}


void MMAP::Unmap() {
    munmap(_mmap, len);
}


MMappedFileAccessor::MMappedFileAccessor(std::string &path) : m_path(path) {
    // BNLogInfo("%s", path.c_str());
    m_mmap.fd = fopen(path.c_str(), "r");
    if (m_mmap.fd == nullptr) {
        BNLogError("Couldn't read file at %s", path.c_str());
        throw MemoryException();
    }
    m_mmap.Map();
}

MMappedFileAccessor::~MMappedFileAccessor() {
    BNLogDebug("Unmapping %s", m_path.c_str());
    m_mmap.Unmap();
    fclose(m_mmap.fd);
}

std::string MMappedFileAccessor::ReadNullTermString(size_t address) {
    if (address > m_mmap.len)
        return "";
    return {(char *) (&((uint8_t *) m_mmap._mmap)[address])};
}

uint8_t MMappedFileAccessor::ReadUChar(size_t address) {
    return ((uint8_t *) (&(((uint8_t *) m_mmap._mmap)[address])))[0];
}

int8_t MMappedFileAccessor::ReadChar(size_t address) {
    return ((int8_t *) (&(((uint8_t *) m_mmap._mmap)[address])))[0];
}

uint16_t MMappedFileAccessor::ReadUShort(size_t address) {
    return ((uint16_t *) (&(((uint8_t *) m_mmap._mmap)[address])))[0];
}

int16_t MMappedFileAccessor::ReadShort(size_t address) {
    return ((int16_t *) (&(((uint8_t *) m_mmap._mmap)[address])))[0];
}

uint32_t MMappedFileAccessor::ReadUInt32(size_t address) {
    return ((uint32_t *) (&(((uint8_t *) m_mmap._mmap)[address])))[0];
}

int32_t MMappedFileAccessor::ReadInt32(size_t address) {
    return ((int32_t *) (&(((uint8_t *) m_mmap._mmap)[address])))[0];
}

uint64_t MMappedFileAccessor::ReadULong(size_t address) {
    return ((uint64_t *) (&(((uint8_t *) m_mmap._mmap)[address])))[0];
}

int64_t MMappedFileAccessor::ReadLong(size_t address) {
    return ((int64_t *) (&(((uint8_t *) m_mmap._mmap)[address])))[0];
}

BinaryNinja::DataBuffer *MMappedFileAccessor::ReadBuffer(size_t address, size_t length) {
    return new BinaryNinja::DataBuffer((void *) &(((uint8_t *) m_mmap._mmap)[address]), length);
}

void MMappedFileAccessor::Read(void *dest, size_t address, size_t length) {
    size_t max = m_mmap.len;
    if (address > max)
        return;
    while (address + length > max)
        length--;
    memcpy(dest, (void *) &(((uint8_t *) m_mmap._mmap)[address]), length);
}


VM::VM(size_t pageSize, bool safe) : m_pageSize(pageSize) {
    unsigned bits, var = (m_pageSize - 1 < 0) ? -(m_pageSize - 1) : m_pageSize - 1;
    for (bits = 0; var != 0; ++bits) var >>= 1;
    m_pageSizeBits = bits;
}

VM::~VM() {
    std::set<MMappedFileAccessor *> mmaps;
    for (const auto &[key, value]: m_map) {
        mmaps.insert(value.file);
    }

    for (auto m: mmaps) {
        delete m;
    }
}


void VM::MapPages(size_t vm_address, size_t fileoff, size_t size, MMappedFileAccessor *file) {
    // The mappings provided for shared caches will always be page aligned.
    // We can use this to our advantage and gain considerable performance via page tables.
    // This could probably be sped up if c++ were avoided?
    // We want to create a map of page -> file offset

    if (vm_address % m_pageSize != 0 || size % m_pageSize != 0) {
        throw MappingPageAlignmentException();
    }

    size_t pagesRemainingCount = size / m_pageSize;
    for (size_t i = 0; i < size; i += m_pageSize) {
        // Our pages will be delimited by shifting off the page size
        // So, 0x12345000 will become 0x12345 (assuming m_pageSize is 0x1000)
        auto page = (vm_address + (i)) >> m_pageSizeBits;
        if (m_map.count(page) != 0) {
            if (m_safe) {
                BNLogWarn("Remapping page 0x%lx (i == 0x%lx) (a: 0x%zx, f: 0x%zx)", page, i, vm_address, fileoff);
                throw MappingCollisionException();
            }
        }
        m_map[page] = {.file = file, .fileOffset = i + fileoff};
    }
}

inline std::pair<PageMapping, size_t> VM::MappingAtAddress(size_t address) {
    // Get the page (e.g. 0x12345678 will become 0x12345 on 0x1000 aligned caches)
    auto page = address >> m_pageSizeBits;
    if (auto f = m_map.find(page); f != m_map.end()) {
        // The PageMapping object returned contains the page, and more importantly, the file pointer (there can be multiple in newer caches)
        // This is relevant for reading out the data in the rest of this file.
        // The second item in this pair is created by taking the fileOffset (which will be a page but with the trailing bits (e.g. 0x12345000)
        //      and will add the "extra" bits lopped off when determining the page. (e.g. 0x12345678 -> 0x678)
        return {f->second, f->second.fileOffset + (address & (m_pageSize - 1))};
    }
    /*
#ifndef NDEBUG
    BNLogError("Tried to access page %lx", page);
    BNLogError("Address: %lx", address);

    raise(2); // SIGINT
#endif
*/
    throw MappingReadException();
}


bool VM::AddressIsMapped(uint64_t address) {
    try {
        MappingAtAddress(address);
        return true;
    }
    catch (...) {

    }
    return false;
}


uint64_t VMReader::ReadULEB128(size_t limit) {
    uint64_t result = 0;
    int bit = 0;
    auto mapping = m_vm->MappingAtAddress(m_cursor);
    auto fileCursor = mapping.second;
    auto fileLimit = fileCursor + (limit - m_cursor);
    auto *fileBuff = (uint8_t *) mapping.first.file->Data();
    do {
        if (fileCursor >= fileLimit)
            return -1;
        uint64_t slice = ((uint64_t *) &((fileBuff)[fileCursor]))[0] & 0x7f;
        if (bit > 63)
            return -1;
        else {
            result |= (slice << bit);
            bit += 7;
        }
    } while (((uint64_t *) &(fileBuff[fileCursor++]))[0] & 0x80);
    return result;
}


int64_t VMReader::ReadSLEB128(size_t limit) {
    uint8_t cur;
    int64_t value = 0;
    size_t shift = 0;

    auto mapping = m_vm->MappingAtAddress(m_cursor);
    auto fileCursor = mapping.second;
    auto fileLimit = fileCursor + (limit - m_cursor);
    auto *fileBuff = (uint8_t *) mapping.first.file->Data();

    while (fileCursor < fileLimit) {
        cur = ((uint64_t *) &((fileBuff)[fileCursor]))[0];
        fileCursor++;
        value |= (cur & 0x7f) << shift;
        shift += 7;
        if ((cur & 0x80) == 0)
            break;
    }
    value = (value << (64 - shift)) >> (64 - shift);
    return value;
}

std::string VM::ReadNullTermString(size_t address) {
    auto mapping = MappingAtAddress(address);
    return mapping.first.file->ReadNullTermString(mapping.second);
}

uint8_t VM::ReadUChar(size_t address) {
    auto mapping = MappingAtAddress(address);
    return mapping.first.file->ReadUChar(mapping.second);
}

int8_t VM::ReadChar(size_t address) {
    auto mapping = MappingAtAddress(address);
    return mapping.first.file->ReadChar(mapping.second);
}

uint16_t VM::ReadUShort(size_t address) {
    auto mapping = MappingAtAddress(address);
    return mapping.first.file->ReadUShort(mapping.second);
}

int16_t VM::ReadShort(size_t address) {
    auto mapping = MappingAtAddress(address);
    return mapping.first.file->ReadShort(mapping.second);
}

uint32_t VM::ReadUInt32(size_t address) {
    auto mapping = MappingAtAddress(address);
    return mapping.first.file->ReadUInt32(mapping.second);
}

int32_t VM::ReadInt32(size_t address) {
    auto mapping = MappingAtAddress(address);
    return mapping.first.file->ReadInt32(mapping.second);
}

uint64_t VM::ReadULong(size_t address) {
    auto mapping = MappingAtAddress(address);
    return mapping.first.file->ReadULong(mapping.second);
}

int64_t VM::ReadLong(size_t address) {
    auto mapping = MappingAtAddress(address);
    return mapping.first.file->ReadLong(mapping.second);
}

BinaryNinja::DataBuffer *VM::ReadBuffer(size_t addr, size_t length) {
    auto mapping = MappingAtAddress(addr);
    return mapping.first.file->ReadBuffer(mapping.second, length);
}


void VM::Read(void *dest, size_t addr, size_t length) {
    auto mapping = MappingAtAddress(addr);
    mapping.first.file->Read(dest, mapping.second, length);
}

VMReader::VMReader(VM *vm, size_t addressSize) : m_vm(vm), m_cursor(0), m_addressSize(addressSize) {
}


void VMReader::Seek(size_t address) {
    m_cursor = address;
}

void VMReader::SeekRelative(size_t offset) {
    m_cursor += offset;
}

std::string VMReader::ReadNullTermString(size_t address) {
    auto mapping = m_vm->MappingAtAddress(address);
    return mapping.first.file->ReadNullTermString(mapping.second);
}

uint8_t VMReader::ReadUChar(size_t address) {
    auto mapping = m_vm->MappingAtAddress(address);
    m_cursor = address + 1;
    return mapping.first.file->ReadUChar(mapping.second);
}

int8_t VMReader::ReadChar(size_t address) {
    auto mapping = m_vm->MappingAtAddress(address);
    m_cursor = address + 1;
    return mapping.first.file->ReadChar(mapping.second);
}

uint16_t VMReader::ReadUShort(size_t address) {
    auto mapping = m_vm->MappingAtAddress(address);
    m_cursor = address + 2;
    return mapping.first.file->ReadUShort(mapping.second);
}

int16_t VMReader::ReadShort(size_t address) {
    auto mapping = m_vm->MappingAtAddress(address);
    m_cursor = address + 2;
    return mapping.first.file->ReadShort(mapping.second);
}

uint32_t VMReader::ReadUInt32(size_t address) {
    auto mapping = m_vm->MappingAtAddress(address);
    m_cursor = address + 4;
    return mapping.first.file->ReadUInt32(mapping.second);
}

int32_t VMReader::ReadInt32(size_t address) {
    auto mapping = m_vm->MappingAtAddress(address);
    m_cursor = address + 4;
    return mapping.first.file->ReadInt32(mapping.second);
}

uint64_t VMReader::ReadULong(size_t address) {
    auto mapping = m_vm->MappingAtAddress(address);
    m_cursor = address + 8;
    return mapping.first.file->ReadULong(mapping.second);
}

int64_t VMReader::ReadLong(size_t address) {
    auto mapping = m_vm->MappingAtAddress(address);
    m_cursor = address + 8;
    return mapping.first.file->ReadLong(mapping.second);
}


size_t VMReader::ReadPointer(size_t address) {
    if (m_addressSize == 8)
        return ReadULong(address);
    else if (m_addressSize == 4)
        return ReadUInt32(address);

    // no idea what horrible arch we have, should probably die here.
    return 0;
}


size_t VMReader::ReadPointer() {
    if (m_addressSize == 8)
        return ReadULong();
    else if (m_addressSize == 4)
        return ReadUInt32();

    return 0;
}

BinaryNinja::DataBuffer *VMReader::ReadBuffer(size_t length) {
    auto mapping = m_vm->MappingAtAddress(m_cursor);
    m_cursor += length;
    return mapping.first.file->ReadBuffer(mapping.second, length);
}

BinaryNinja::DataBuffer *VMReader::ReadBuffer(size_t addr, size_t length) {
    auto mapping = m_vm->MappingAtAddress(addr);
    m_cursor = addr + length;
    return mapping.first.file->ReadBuffer(mapping.second, length);
}

void VMReader::Read(void *dest, size_t length) {
    auto mapping = m_vm->MappingAtAddress(m_cursor);
    m_cursor += length;
    mapping.first.file->Read(dest, mapping.second, length);
}

void VMReader::Read(void *dest, size_t addr, size_t length) {
    auto mapping = m_vm->MappingAtAddress(addr);
    m_cursor = addr + length;
    mapping.first.file->Read(dest, mapping.second, length);
}


uint8_t VMReader::ReadUChar() {
    auto mapping = m_vm->MappingAtAddress(m_cursor);
    m_cursor += 1;
    return mapping.first.file->ReadUChar(mapping.second);
}

int8_t VMReader::ReadChar() {
    auto mapping = m_vm->MappingAtAddress(m_cursor);
    m_cursor += 1;
    return mapping.first.file->ReadChar(mapping.second);
}

uint16_t VMReader::ReadUShort() {
    auto mapping = m_vm->MappingAtAddress(m_cursor);
    m_cursor += 2;
    return mapping.first.file->ReadUShort(mapping.second);
}

int16_t VMReader::ReadShort() {
    auto mapping = m_vm->MappingAtAddress(m_cursor);
    m_cursor += 2;
    return mapping.first.file->ReadShort(mapping.second);
}

uint32_t VMReader::ReadUInt32() {
    auto mapping = m_vm->MappingAtAddress(m_cursor);
    m_cursor += 4;
    return mapping.first.file->ReadUInt32(mapping.second);
}

int32_t VMReader::ReadInt32() {
    auto mapping = m_vm->MappingAtAddress(m_cursor);
    m_cursor += 4;
    return mapping.first.file->ReadInt32(mapping.second);
}

uint64_t VMReader::ReadULong() {
    auto mapping = m_vm->MappingAtAddress(m_cursor);
    m_cursor += 8;
    return mapping.first.file->ReadULong(mapping.second);
}

int64_t VMReader::ReadLong() {
    auto mapping = m_vm->MappingAtAddress(m_cursor);
    m_cursor += 8;
    return mapping.first.file->ReadLong(mapping.second);
}


namespace fs = std::filesystem;

DSCViewType *g_dscViewType;
DSCRawViewType *g_dscRawViewType;


MetricDurationItem *Metrics::AddMetric(std::string itemTitle) {
    auto *item = new MetricDurationItem;
    item->itemTitle = std::move(itemTitle);
    durationItems.push_back(item);
    return item;
}

std::pair<CacheImage, std::vector<LoadedImageBuffer>> SharedCache::LoadImageByBaseName(std::string &installName) {
    if (auto iname = m_baseNames.find(installName); iname != m_baseNames.end())
        LoadImageByInstallName(iname->second);
    return {};
}


std::pair<CacheImage, std::vector<LoadedImageBuffer>> SharedCache::LoadImageByInstallName(std::string &installName) {
    if (auto img = m_images.find(installName); img != m_images.end()) {
        std::vector<LoadedImageBuffer> loadSegments;
        auto *reader = new VMReader(m_vm);

        reader->Seek(img->second.headerAddress);
        size_t magic = reader->ReadUInt32();

        // VM Logic Sanity ---
        if (magic != MH_MAGIC_64 && magic != MH_CIGAM_64 && magic != MH_CIGAM && magic != MH_MAGIC) {
            BNLogError("Bad magic for install name '%s' at vm addr %llx ", installName.c_str(),
                       img->second.headerAddress);
            BNLogError("Map %llx -> %lx", img->second.headerAddress,
                       m_vm->MappingAtAddress(img->second.headerAddress).second);
            throw MappingReadException(); // whatever
        } // ---

        reader->Seek(img->second.headerAddress);
        auto sections = MinLoader::SectionsInHeader(reader);

        loadSegments.reserve(sections.size());
        for (auto sect: sections) {
            loadSegments.push_back({sect.start, sect.fstart, sect.initprot, m_vm->ReadBuffer(sect.start, sect.length)});
        }

        return {img->second, loadSegments};
    }
    return {};
}


SharedCacheFormat SharedCache::GetCacheFormat(MMappedFileAccessor *file) {
    dyld_cache_header header{};
    size_t header_size = file->ReadUInt32(16);
    file->Read(&header, 0, std::min(header_size, sizeof(dyld_cache_header)));

    if (header.imagesCountOld != 0)
        return RegularCacheFormat;

    size_t subCacheOff = offsetof(struct dyld_cache_header, subCacheArrayOffset);
    size_t headerEnd = header.mappingOffset;
    if (headerEnd > subCacheOff) {
        if (header.cacheType != 2)
        {
            if (std::filesystem::exists(file->Path() + ".01"))
                return LargeCacheFormat;
            return SplitCacheFormat;
        }
        else
            return iOS16CacheFormat;
    }

    return RegularCacheFormat;
}


SharedCache *SharedCache::LoadCache(MMappedFileAccessor *file) {
    auto format = GetCacheFormat(file);

    dyld_cache_header header{};
    size_t header_size = file->ReadUInt32(16);
    file->Read(&header, 0, std::min(header_size, sizeof(dyld_cache_header)));

    BNLogError("fmt: %d", format);

    switch (format) {
        case RegularCacheFormat: {
            auto cache = new SharedCache();
            cache->format = format;
            cache->m_vm = new VM(0x4000); // TODO: way to absolutely determine page size
            cache->m_baseFile = file;

            dyld_cache_mapping_info mapping{};

            for (size_t i = 0; i < header.mappingCount; i++) {
                file->Read(&mapping, header.mappingOffset + (i * sizeof(mapping)),
                           sizeof(mapping));
                cache->m_vm->MapPages(mapping.address, mapping.fileOffset, mapping.size, file);
            }

            dyld_cache_image_info image{};

            for (size_t i = 0; i < header.imagesCountOld; i++) {
                file->Read(&image, header.imagesOffsetOld + (i * sizeof(image)), sizeof(image));
                auto iname = file->ReadNullTermString(image.pathFileOffset);
                auto addr = image.address;

                cache->m_installNames.push_back(iname);
                cache->m_images[iname] = {.installName = iname, .headerAddress = addr};
                auto baseName = fs::path(iname).filename().string();
                cache->m_baseNames[baseName] = iname;
            }

            return cache;
        }
        case LargeCacheFormat: {
            auto cache = new SharedCache();
            cache->format = format;
            cache->m_vm = new VM(0x4000);
            cache->m_baseFile = file;

            // First, map our main cache.

            dyld_cache_mapping_info mapping{}; // We're going to reuse this for all of the mappings. We only need it briefly.

            for (size_t i = 0; i < header.mappingCount; i++) {
                file->Read(&mapping, header.mappingOffset + (i * sizeof(mapping)),
                           sizeof(mapping));
                cache->m_vm->MapPages(mapping.address, mapping.fileOffset, mapping.size, file);
            }

            auto mainFileName = file->Path();
            auto subCacheCount = header.subCacheArrayCount;
            dyld_subcache_entry2 entry{};

            for (size_t i = 0; i < subCacheCount; i++) {

                file->Read(&entry, header.subCacheArrayOffset + (i * sizeof(dyld_subcache_entry2)),
                           sizeof(dyld_subcache_entry2));

                std::string subCachePath;
                if (std::string(entry.fileExtension).find('.') != std::string::npos)
                    subCachePath = mainFileName + entry.fileExtension;
                else
                    subCachePath = mainFileName + "." + entry.fileExtension;

                auto subCacheFile = new MMappedFileAccessor(subCachePath);

                cache->m_nonBaseFiles.emplace_back(entry.address, subCacheFile);

                auto header_size = subCacheFile->ReadUInt32(16);
                dyld_cache_header header{};
                subCacheFile->Read(&header, 0, header_size);

                dyld_cache_mapping_info subCacheMapping{};

                for (size_t j = 0; j < header.mappingCount; j++) {

                    subCacheFile->Read(&subCacheMapping, header.mappingOffset + (j * sizeof(subCacheMapping)),
                                       sizeof(subCacheMapping));
                    cache->m_vm->MapPages(subCacheMapping.address, subCacheMapping.fileOffset, subCacheMapping.size,
                                          subCacheFile);

                    cache->m_mappedRanges.push_back({subCacheMapping.address, subCacheMapping.size,
                                                     subCacheMapping.address + subCacheMapping.size, subCachePath});

                    if (std::string(entry.fileExtension).find("dylddata") != std::string::npos) {
                        cache->m_dyldDataSections.push_back({subCacheMapping.address, subCacheMapping.size,
                                                             subCacheMapping.address + subCacheMapping.size, ""});
                    }
                    // BNLogInfo("%s || %llx -> %llx", subCachePath.c_str(), subCacheMapping.address,
                    //          subCacheMapping.address + subCacheMapping.size);
                }
            }
            dyld_cache_image_info image{};

            for (size_t i = 0; i < header.imagesCount; i++) {
                file->Read(&image, header.imagesOffset + (i * sizeof(image)), sizeof(image));
                auto iname = file->ReadNullTermString(image.pathFileOffset);
                auto addr = image.address;

                cache->m_installNames.push_back(iname);
                cache->m_images[iname] = {.installName = iname, .headerAddress = addr};
                auto baseName = fs::path(iname).filename().string();
                cache->m_baseNames[baseName] = iname;
            }

            return cache;
        }
        case SplitCacheFormat: {
            auto cache = new SharedCache();
            cache->format = format;
            cache->m_vm = new VM(0x4000);
            cache->m_baseFile = file;

            // First, map our main cache.

            dyld_cache_mapping_info mapping{}; // We're going to reuse this for all of the mappings. We only need it briefly.

            for (size_t i = 0; i < header.mappingCount; i++) {
                file->Read(&mapping, header.mappingOffset + (i * sizeof(mapping)),
                           sizeof(mapping));
                cache->m_vm->MapPages(mapping.address, mapping.fileOffset, mapping.size, file);
            }

            auto mainFileName = file->Path();
            auto subCacheCount = header.subCacheArrayCount;

            for (size_t i = 1; i <= subCacheCount; i++) {
                auto subCachePath = mainFileName + "." + std::to_string(i);
                auto subCacheFile = new MMappedFileAccessor(subCachePath);
                auto header_size = subCacheFile->ReadUInt32(16);
                dyld_cache_header header{};
                subCacheFile->Read(&header, 0, header_size);

                dyld_cache_mapping_info subCacheMapping{};

                for (size_t j = 0; j < header.mappingCount; j++) {
                    subCacheFile->Read(&subCacheMapping, header.mappingOffset + (j * sizeof(subCacheMapping)),
                                       sizeof(subCacheMapping));
                    cache->m_vm->MapPages(subCacheMapping.address, subCacheMapping.fileOffset, subCacheMapping.size,
                                          subCacheFile);
                }
            }

            // Load .symbols subcache

            auto subCachePath = mainFileName + ".symbols";
            auto subCacheFile = new MMappedFileAccessor(subCachePath);

            auto subcache_header_size = subCacheFile->ReadUInt32(16);
            dyld_cache_header subcacheHeader{};
            subCacheFile->Read(&subcacheHeader, 0, subcache_header_size);

            dyld_cache_mapping_info subCacheMapping{};

            for (size_t j = 0; j < subcacheHeader.mappingCount; j++) {
                subCacheFile->Read(&subCacheMapping, subcacheHeader.mappingOffset + (j * sizeof(subCacheMapping)),
                                   sizeof(subCacheMapping));
                cache->m_vm->MapPages(subCacheMapping.address, subCacheMapping.fileOffset, subCacheMapping.size,
                                      subCacheFile);
            }

            // Load Images

            dyld_cache_image_info image{};

            BinaryNinja::LogInfo("%d Images in cache", header.imagesCount);
            for (size_t i = 0; i < header.imagesCount; i++) {
                file->Read(&image, header.imagesOffset + (i * sizeof(image)), sizeof(image));
                auto iname = file->ReadNullTermString(image.pathFileOffset);
                auto addr = image.address;

                cache->m_installNames.push_back(iname);
                cache->m_images[iname] = {.installName = iname, .headerAddress = addr};
                auto baseName = fs::path(iname).filename().string();
                cache->m_baseNames[baseName] = iname;
            }

            return cache;
        }
        case iOS16CacheFormat: {
            auto cache = new SharedCache();
            cache->format = format;
            cache->m_vm = new VM(0x4000);
            cache->m_baseFile = file;

            // First, map our main cache.

            dyld_cache_mapping_info mapping{};

            for (size_t i = 0; i < header.mappingCount; i++) {
                file->Read(&mapping, header.mappingOffset + (i * sizeof(mapping)),
                           sizeof(mapping));
                cache->m_vm->MapPages(mapping.address, mapping.fileOffset, mapping.size, file);
                cache->m_mappedRanges.push_back(
                        {mapping.address, mapping.size, mapping.address + mapping.size, file->Path()});
            }

            auto mainFileName = file->Path();
            auto subCacheCount = header.subCacheArrayCount;

            dyld_subcache_entry2 entry{};

            for (size_t i = 0; i < subCacheCount; i++) {

                file->Read(&entry, header.subCacheArrayOffset + (i * sizeof(dyld_subcache_entry2)),
                           sizeof(dyld_subcache_entry2));

                std::string subCachePath;
                if (std::string(entry.fileExtension).find('.') != std::string::npos)
                    subCachePath = mainFileName + entry.fileExtension;
                else
                    subCachePath = mainFileName + "." + entry.fileExtension;

                auto subCacheFile = new MMappedFileAccessor(subCachePath);

                cache->m_nonBaseFiles.emplace_back(entry.address, subCacheFile);

                auto header_size = subCacheFile->ReadUInt32(16);
                dyld_cache_header header{};
                subCacheFile->Read(&header, 0, header_size);

                dyld_cache_mapping_info subCacheMapping{};

                for (size_t j = 0; j < header.mappingCount; j++) {

                    subCacheFile->Read(&subCacheMapping, header.mappingOffset + (j * sizeof(subCacheMapping)),
                                       sizeof(subCacheMapping));
                    cache->m_vm->MapPages(subCacheMapping.address, subCacheMapping.fileOffset, subCacheMapping.size,
                                          subCacheFile);

                    cache->m_mappedRanges.push_back({subCacheMapping.address, subCacheMapping.size,
                                                     subCacheMapping.address + subCacheMapping.size, subCachePath});

                    if (std::string(entry.fileExtension).find("dylddata") != std::string::npos) {
                        cache->m_dyldDataSections.push_back({subCacheMapping.address, subCacheMapping.size,
                                                             subCacheMapping.address + subCacheMapping.size, ""});
                    }
                    // BNLogInfo("%s || %llx -> %llx", subCachePath.c_str(), subCacheMapping.address,
                    //          subCacheMapping.address + subCacheMapping.size);
                }
            }

            // Load .symbols subcache
            try {
                auto subCachePath = mainFileName + ".symbols";
                auto subCacheFile = new MMappedFileAccessor(subCachePath);
                auto header_size = subCacheFile->ReadUInt32(16);
                dyld_cache_header header{};
                subCacheFile->Read(&header, 0, header_size);

                dyld_cache_mapping_info subCacheMapping{};

                for (size_t j = 0; j < header.mappingCount; j++) {
                    subCacheFile->Read(&subCacheMapping, header.mappingOffset + (j * sizeof(subCacheMapping)),
                                       sizeof(subCacheMapping));
                    cache->m_vm->MapPages(subCacheMapping.address, subCacheMapping.fileOffset, subCacheMapping.size,
                                          subCacheFile);
                    cache->m_mappedRanges.push_back({subCacheMapping.address, subCacheMapping.size,
                                                     subCacheMapping.address + subCacheMapping.size, subCachePath});
                }
            } catch (...) {

            }

            // Load Images

            dyld_cache_image_info image{};

            for (size_t i = 0; i < header.imagesCount; i++) {
                file->Read(&image, header.imagesOffset + (i * sizeof(image)), sizeof(image));
                auto iname = file->ReadNullTermString(image.pathFileOffset);
                auto addr = image.address;

                cache->m_installNames.push_back(iname);
                cache->m_images[iname] = {.installName = iname, .headerAddress = addr};
                auto baseName = fs::path(iname).filename().string();
                cache->m_baseNames[baseName] = iname;
            }

            return cache;
        }
    }
}


static uint64_t ReadULEB128(MMappedFileAccessor *reader, size_t &cursor, size_t limit) {
    uint64_t result = 0;
    int bit = 0;
    do {
        if (cursor >= limit)
            return -1;

        uint64_t slice = reader->ReadUChar(cursor) & 0x7f;

        if (bit > 63)
            return -1;
        else {
            result |= (slice << bit);
            bit += 7;
        }
    } while (reader->ReadUChar(cursor++) & 0x80);
    return result;
}


static int64_t ReadSLEB128(MMappedFileAccessor *reader, size_t &cursor, size_t limit) {
    uint8_t cur;
    int64_t value = 0;
    size_t shift = 0;
    while (cursor < limit) {
        cur = reader->ReadUChar(cursor);
        cursor++;
        value |= (cur & 0x7f) << shift;
        shift += 7;
        if ((cur & 0x80) == 0)
            break;
    }
    value = (value << (64 - shift)) >> (64 - shift);
    return value;
}

std::vector<ImageSectionRange> MinLoader::SectionsInHeader(VMReader *reader) {
    size_t headerStart = reader->Offset();
    auto magic = reader->ReadUInt32(headerStart);
    bool is64 = (magic == MH_MAGIC_64 || magic == MH_CIGAM_64);

    std::vector<ImageSectionRange> sections;

    if (is64) {
        mach_header_64 header{};
        reader->Read(&header, headerStart, sizeof(mach_header_64));
        size_t off = headerStart + sizeof(mach_header_64);
        for (size_t i = 0; i < header.ncmds; i++) {
            size_t lc = reader->ReadUInt32(off);
            size_t bump = reader->ReadUInt32();
            if (lc == LC_SEGMENT_64) {
                segment_command_64 cmd{};
                reader->Read(&cmd, off, sizeof(segment_command_64));

                sections.push_back(
                        {cmd.vmaddr, cmd.vmsize, cmd.fileoff, cmd.filesize, static_cast<uint64_t>(cmd.initprot)});
            }
            off += bump;
        }
    } else {
        mach_header header{};
        reader->Read(&header, headerStart, sizeof(mach_header));
        size_t off = headerStart + sizeof(mach_header);
        for (size_t i = 0; i < header.ncmds; i++) {
            size_t lc = reader->ReadUInt32(off);
            size_t bump = reader->ReadUInt32();
            if (lc == LC_SEGMENT) {
                segment_command cmd{};
                reader->Read(&cmd, off, sizeof(segment_command));

                sections.push_back(
                        {cmd.vmaddr, cmd.vmsize, cmd.fileoff, cmd.filesize, static_cast<uint64_t>(cmd.initprot)});
            }
            off += bump;
        }
    }

    return sections;
}

std::pair<linkedit_data_command, uint64_t> MinLoader::ExportTrieAndLinkeditBaseForImage(VMReader *reader) {
    size_t headerStart = reader->Offset();
    auto magic = reader->ReadUInt32(headerStart);
    bool is64 = (magic == MH_MAGIC_64 || magic == MH_CIGAM_64);

    std::pair<linkedit_data_command, uint64_t> info = {{0, 0}, 0};

    if (is64) {
        mach_header_64 header{};
        reader->Read(&header, headerStart, sizeof(mach_header_64));
        size_t off = headerStart + sizeof(mach_header_64);
        for (size_t i = 0; i < header.ncmds; i++) {
            size_t lc = reader->ReadUInt32(off);
            size_t bump = reader->ReadUInt32();
            if (lc == LC_DYLD_INFO) {
                dyld_info_command cmd{};
                reader->Read(&cmd, off, sizeof(dyld_info_command));
                info.first = {.dataoff = cmd.export_off, .datasize = cmd.export_size};
            }
            if (lc == LC_DYLD_EXPORTS_TRIE) {
                linkedit_data_command cmd{};
                reader->Read(&cmd, off, sizeof(linkedit_data_command));
                info.first = cmd;
            }
            if (lc == LC_SEGMENT_64) {
                segment_command_64 cmd{};
                reader->Read(&cmd, off, sizeof(segment_command_64));
                if (strncmp(cmd.segname, "__LINKEDIT", 10) == 0)
                    info.second = cmd.vmaddr;
            }
            off += bump;
        }
    } else {
        mach_header header{};
        reader->Read(&header, headerStart, sizeof(mach_header));
        size_t off = headerStart + sizeof(mach_header);
        for (size_t i = 0; i < header.ncmds; i++) {
            size_t lc = reader->ReadUInt32(off);
            size_t bump = reader->ReadUInt32();
            if (lc == LC_DYLD_INFO) {
                dyld_info_command cmd{};
                reader->Read(&cmd, off, sizeof(dyld_info_command));
                info.first = {.dataoff = cmd.export_off, .datasize = cmd.export_size};
            }
            if (lc == LC_DYLD_EXPORTS_TRIE) {
                linkedit_data_command cmd{};
                reader->Read(&cmd, off, sizeof(linkedit_data_command));
                info.first = cmd;
            }
            if (lc == LC_SEGMENT) {
                segment_command cmd{};
                reader->Read(&cmd, off, sizeof(segment_command));
                if (strncmp(cmd.segname, "__LINKEDIT", 10) == 0)
                    info.second = cmd.vmaddr;
            }
            off += bump;
        }
    }

    return info;
}

std::pair<symtab_command, segment_command_64> MinLoader::SymtabAndLinkeditBaseForImage(VMReader* reader)
{size_t headerStart = reader->Offset();
    auto magic = reader->ReadUInt32(headerStart);
    bool is64 = (magic == MH_MAGIC_64 || magic == MH_CIGAM_64);

    std::pair<symtab_command, segment_command_64> info = {{0, 0}, {}};

    if (is64) {
        mach_header_64 header{};
        reader->Read(&header, headerStart, sizeof(mach_header_64));
        size_t off = headerStart + sizeof(mach_header_64);
        for (size_t i = 0; i < header.ncmds; i++) {
            size_t lc = reader->ReadUInt32(off);
            size_t bump = reader->ReadUInt32();

            if (lc == LC_SYMTAB) {
                symtab_command cmd{};
                reader->Read(&cmd, off, sizeof(symtab_command));
                info.first = cmd;
            }
            if (lc == LC_SEGMENT_64) {
                segment_command_64 cmd{};
                reader->Read(&cmd, off, sizeof(segment_command_64));
                if (strncmp(cmd.segname, "__LINKEDIT", 10) == 0)
                    info.second = cmd;
            }
            off += bump;
        }
    } else {
        mach_header header{};
        reader->Read(&header, headerStart, sizeof(mach_header));
        size_t off = headerStart + sizeof(mach_header);
        for (size_t i = 0; i < header.ncmds; i++) {
            size_t lc = reader->ReadUInt32(off);
            size_t bump = reader->ReadUInt32();
            if (lc == LC_SYMTAB) {
                symtab_command cmd{};
                reader->Read(&cmd, off, sizeof(symtab_command));
                info.first = cmd;
            }
            if (lc == LC_SEGMENT) {
                segment_command_64 cmd{};
                reader->Read(&cmd, off, sizeof(segment_command));
                if (strncmp(cmd.segname, "__LINKEDIT", 10) == 0)
                    info.second = cmd;
            }
            off += bump;
        }
    }

    return info;
}

std::pair<QualifiedName, Ref<Type>> FinalizeStructureBuilder(DSCView *bv, StructureBuilder sb, std::string name) {
auto classTypeStruct = sb.Finalize();

QualifiedName classTypeName(name);
auto classTypeId = Type::GenerateAutoTypeId("objc", classTypeName);
auto classType = Type::StructureType(classTypeStruct);
auto classQualName = bv->DefineType(classTypeId, classTypeName, classType);

return {classQualName, classType};
}

inline void DefineTypedef(DSCView *bv, const QualifiedName name, Ref<Type> type) {
auto typeID = Type::GenerateAutoTypeId("objc", name);
bv->DefineType(typeID, name, type);
}


static const std::map<char, std::string> TypeEncodingMap = {{'v', "void"},
                                                            {'c', "char"},
                                                            {'s', "short"},
                                                            {'i', "int"},
                                                            {'l', "long"},
                                                            {'C', "unsigned char"},
                                                            {'S', "unsigned short"},
                                                            {'I', "unsigned int"},
                                                            {'L', "unsigned long"},
                                                            {'f', "float"},
                                                            {'A', "uint8_t"},
                                                            {'b', "BOOL"},
                                                            {'B', "BOOL"},

                                                            {'q', "NSInteger"},
                                                            {'Q', "NSUInteger"},
                                                            {'d', "CGFloat"},
                                                            {'*', "char *"},

                                                            {'@', "id"},
                                                            {':', "SEL"},
                                                            {'#', "objc_class_t"},

                                                            {'?', "void*"},
                                                            {'T', "void*"},};

std::vector<std::string> ObjCTypeParser::parseEncodedType(const std::string &encodedType) {
    std::vector<std::string> result;
    int pointerDepth = 0;

    for (size_t i = 0; i < encodedType.size(); ++i) {
        char c = encodedType[i];

        // K: For example, ^@ is a single type, "id*".
        if (c == '^') {
            pointerDepth++;
            continue;
        }

        // Argument frame size and offset specifiers aren't relevant here; they
        // should just be skipped.
        if (std::isdigit(c))
            continue;

        if (auto it = TypeEncodingMap.find(c); it != TypeEncodingMap.end()) {
            std::string encoding = it->second;
            for (int j = pointerDepth; j > 0; j--)
                encoding += "*";
            pointerDepth = 0;
            result.emplace_back(encoding);
            continue;
        }

        // (Partially) handle quoted type names.
        if (c == '"') {
            while (encodedType[i] != '"')
                i++;

            pointerDepth = 0;
            result.emplace_back("void*");
            continue;
        }

        // (Partially) handle struct types.
        if (c == '{') {
            auto depth = 1;

            while (depth != 0) {
                char d = encodedType[++i];

                if (d == '{')
                    ++depth;
                else if (d == '}')
                    --depth;
            }

            pointerDepth = 0;
            result.emplace_back("void*");
            continue;
        }

        break;
    }

    return result;
}

ObjCProcessing::ObjCProcessing(DSCView *view, SharedCache *cache) : m_cache(cache), m_view(view) {
    m_logger = m_view->IssueSessionLogger("objcLoader");
    m_typesLoaded = false;
    m_customRelativeMethodSelectorBase = std::nullopt;

    auto *reader = m_view->GetReader();

    if (auto objc = m_cache->m_images.find("/usr/lib/libobjc.A.dylib"); objc != m_cache->m_images.end()) {
        auto addr = objc->second.headerAddress;
        uint64_t scoffs_addr = 0;
        size_t scoffs_size = 0;

        mach_header_64 header{};

        header.magic = reader->ReadUInt32(addr);
        header.cputype = reader->ReadInt32();
        header.cpusubtype = reader->ReadInt32();
        header.filetype = reader->ReadUInt32();
        header.ncmds = reader->ReadUInt32();
        header.sizeofcmds = reader->ReadUInt32();
        header.flags = reader->ReadUInt32();

        try {
            size_t loadCommandOffset = 32;
            size_t imageBase = addr;
            size_t cursor;
            cursor = imageBase + loadCommandOffset;
            size_t sectionNum = 0;
            for (size_t i = 0; i < header.ncmds; i++) {
                load_command load{};
                uint64_t curOffset = cursor;
                load.cmd = reader->ReadUInt32(cursor);
                load.cmdsize = reader->ReadUInt32(cursor + 4);
                cursor += 8;
                uint64_t nextOffset = curOffset + load.cmdsize;
                switch (load.cmd) {
                    case LC_SEGMENT_64: {
                        segment_command_64 seg{};
                        reader->Read(&seg, curOffset, sizeof(segment_command_64));
                        char segmentName[17];
                        strncpy(segmentName, seg.segname, 16);
                        segmentName[16] = 0;
                        cursor += (7 * 8);
                        size_t numSections = reader->ReadUInt32(cursor);
                        cursor += 8;
                        for (size_t j = 0; j < numSections; j++) {
                            section_64 sect{};
                            reader->Read(&sect, cursor, sizeof(section_64));
                            char sectName[17];
                            char segName[17];
                            strncpy(sectName, sect.sectname, 16);
                            sectName[16] = 0;
                            // BNLogInfo("  Sect: %s", sectName);
                            strncpy(segName, sect.segname, 16);
                            segName[16] = 0;

                            if (std::string(sectName) == "__objc_scoffs") {
                                size_t vaddr = reader->ReadULong(cursor + 32);
                                size_t size = reader->ReadULong(cursor + 40);
                                scoffs_addr = vaddr;
                                scoffs_size = size;
                            }
                            cursor += (10 * 8);
                        }
                        break;
                    }
                    default:
                        break;
                }

            }
        } catch (...) {

        }

        if (scoffs_size && scoffs_addr) {
            if (scoffs_size == 0x20) {
                m_customRelativeMethodSelectorBase = reader->ReadULong(scoffs_addr) & 0xFFFFFFFFF;
            } else {
                m_customRelativeMethodSelectorBase = reader->ReadULong(scoffs_addr + 8) & 0xFFFFFFFFF;
            }
        }
    }
}


std::vector<DSCObjC::Class *> ObjCProcessing::GetClassList(MachOImage &image) {
    std::vector<DSCObjC::Class *> classes{};
    VMReader *reader = m_view->GetReader();
    std::vector<uint64_t> classPtrs;

    if (auto cl = image.sectionsByName.find("__objc_classlist"); cl != image.sectionsByName.end()) {
        //// BNLogInfo("0x%llx", cl->second.addr);
        reader->Seek(cl->second.addr);
        size_t end = cl->second.addr + cl->second.size;
        for (size_t i = cl->second.addr; i < end; i += 8) {
            classPtrs.push_back(reader->ReadULong(i) & 0x1ffffffff);
        }
    }
    if (classPtrs.empty())
        return {};

    for (auto cpt: classPtrs) {
        //// BNLogInfo("0x%llx", cpt);
        DSCObjC::Class *c = new DSCObjC::Class;
        DSCObjC::ClassRO *ro = new DSCObjC::ClassRO;
        c->isa = reader->ReadULong(cpt) & 0x1ffffffff;
        c->super = reader->ReadULong() & 0x1ffffffff;
        reader->ReadULong();
        reader->ReadULong();
        uint64_t ro_addr = reader->ReadULong() & 0x1ffffffff;

        ro->name = (reader->ReadULong(ro_addr + 24)) & 0x1ffffffff;
        ro->methods = (reader->ReadULong(ro_addr + 32)) & 0x1ffffffff;

        c->ro_data = ro;
        classes.push_back(c);
    }
    return classes;
}


std::vector<DSCObjC::Method> ObjCProcessing::LoadMethodList(uint64_t addr) {
    auto *reader = m_view->GetReader();
    auto flags = reader->ReadUInt32(addr) & 0xffff0000;

    bool rms = flags & 0x80000000;
    bool direct = flags & 0x40000000;

    auto count = reader->ReadUInt32();
    std::vector<DSCObjC::Method> methods;

    for (size_t i = 0; i < count; i++) {
        DSCObjC::Method meth{};
        if (rms) {
            if (m_customRelativeMethodSelectorBase.has_value()) {
                meth.name = m_customRelativeMethodSelectorBase.value() + reader->ReadInt32();
                meth.types = reader->Offset() + reader->ReadInt32();
                meth.imp = reader->Offset() + reader->ReadInt32();
            } else {
                meth.name = reader->Offset() + reader->ReadInt32();
                meth.types = reader->Offset() + reader->ReadInt32();
                meth.imp = reader->Offset() + reader->ReadInt32();
            }
        } else {
            meth.name = reader->ReadULong();
            meth.types = reader->ReadULong();
            meth.imp = reader->ReadULong();
        }

        meth.name = meth.name & 0x1ffffffff;
        meth.types = meth.types & 0x1ffffffff;
        meth.imp = meth.imp & 0x1ffffffff;

        if (!direct) {
            meth.name = m_cache->m_vm->ReadULong(meth.name) & 0x1ffffffff;
        }
        methods.push_back(meth);
    }
    return methods;
}


void ObjCProcessing::ApplyMethodType(std::string className, std::string sel, std::string types, uint64_t imp) {
    std::stringstream r(sel);

    std::string token;
    std::vector<std::string> result;
    while (std::getline(r, token, ':'))
        result.push_back(token);

    auto selectorTokens = result;

    auto typeTokens = ObjCTypeParser::parseEncodedType(types);

    // For safety, ensure out-of-bounds indexing is not about to occur. This has
    // never happened and likely won't ever happen, but crashing the product is
    // generally undesirable, so it's better to be safe than sorry.
    if (selectorTokens.size() > typeTokens.size()) {
        BNLogError("oh no");
        return;
    }

    // Shorthand for formatting an individual "part" of the type signature.
    auto partForIndex = [selectorTokens, typeTokens](size_t i) {
        std::string argName;

        // Indices 0, 1, and 2 are the function return type, self parameter, and
        // selector parameter, respectively. Indices 3+ are the actual
        // arguments to the function.
        if (i == 0)
            argName = "";
        else if (i == 1)
            argName = "self";
        else if (i == 2)
            argName = "sel";
        else if (i - 3 < selectorTokens.size())
            argName = selectorTokens[i - 3];

        return typeTokens[i] + " " + argName;
    };

    // Build the type string for the method.
    std::string typeString;
    for (size_t i = 0; i < typeTokens.size(); ++i) {
        std::string suffix;
        auto part = partForIndex(i);

        // The underscore being used as the function name here is critically
        // important as Clang will not parse the type string correctly---unlike
        // the old type parser---if there is no function name. The underscore
        // itself isn't special, and will not end up being used as the function
        // name in either case.
        if (i == 0)
            suffix = " _(";
        else if (i == typeTokens.size() - 1)
            suffix = ")";
        else
            suffix = ", ";

        typeString += part + suffix;
    }
    typeString += ";";

    std::string errors;
    TypeParserResult tpResult;
    auto ok = m_view->ParseTypesFromSource(typeString, {}, {}, tpResult, errors);
    if (ok && !tpResult.functions.empty()) {
        auto functionType = tpResult.functions[0].type;

        // k: we are not in workflow phase so we need to define this here ourself.
        m_view->AddFunctionForAnalysis(m_view->GetDefaultPlatform(), imp);

        // Search for the method's implementation function; apply the type if found.
        if (auto f = m_view->GetAnalysisFunction(m_view->GetDefaultPlatform(), imp))
            f->SetUserType(functionType);
    }

    // TODO: Use '+' or '-' conditionally once class methods are supported. For
    // right now, only instance methods are analyzed and we can just use '-'.
    auto name = "-[" + className + " " + sel + "]";
    m_view->DefineUserSymbol(new Symbol(FunctionSymbol, name, imp));
}


void ObjCProcessing::LoadObjCMetadata(MachOImage &image) {
    if (!m_typesLoaded)
        LoadTypes();
    auto classes = GetClassList(image);

    for (auto c: classes) {
        //// BNLogInfo("%s", m_cache->m_vm->ReadNullTermString(c->ro_data->name).c_str());
        try {

            auto meths = LoadMethodList(c->ro_data->methods);

            for (auto m: meths) {
                //// BNLogInfo(":: %s", m_cache->m_vm->ReadNullTermString(m.name).c_str());
                ApplyMethodType(m_cache->m_vm->ReadNullTermString(c->ro_data->name),
                                m_cache->m_vm->ReadNullTermString(m.name), m_cache->m_vm->ReadNullTermString(m.types),
                                m.imp);
            }
        } catch (...) {
            BNLogError("failz xD");
        }
    }
}

void ObjCProcessing::LoadTypes() {
    std::unique_lock<std::mutex> lock(m_typeDefMutex);

#pragma clang diagnostic push
#pragma ide diagnostic ignored "ConstantConditionsOC"
    if (m_typesLoaded)
        return;
#pragma clang diagnostic pop

    size_t addrSize = m_view->GetAddressSize();

    DefineTypedef(m_view, {CustomTypes::TaggedPointer}, Type::PointerType(addrSize, Type::VoidType()));
    DefineTypedef(m_view, {CustomTypes::FastPointer}, Type::PointerType(addrSize, Type::VoidType()));
    DefineTypedef(m_view, {CustomTypes::RelativePointer}, Type::IntegerType(4, true));

    DefineTypedef(m_view, {"id"}, Type::PointerType(addrSize, Type::VoidType()));
    DefineTypedef(m_view, {"SEL"}, Type::PointerType(addrSize, Type::IntegerType(1, false)));

    DefineTypedef(m_view, {"BOOL"}, Type::IntegerType(1, false));
    DefineTypedef(m_view, {"NSInteger"}, Type::IntegerType(addrSize, true));
    DefineTypedef(m_view, {"NSUInteger"}, Type::IntegerType(addrSize, false));
    DefineTypedef(m_view, {"CGFloat"}, Type::FloatType(addrSize));

    StructureBuilder cfstringStructBuilder;
    cfstringStructBuilder.AddMember(Type::PointerType(addrSize, Type::VoidType()), "isa");
    cfstringStructBuilder.AddMember(Type::IntegerType(addrSize, false), "flags");
    cfstringStructBuilder.AddMember(Type::PointerType(addrSize, Type::VoidType()), "data");
    cfstringStructBuilder.AddMember(Type::IntegerType(addrSize, false), "size");
    auto type = FinalizeStructureBuilder(m_view, cfstringStructBuilder, "CFString");
    m_types.CFString = type.first;

    StructureBuilder methodEntry;
    methodEntry.AddMember(Type::IntegerType(4, true), "name");
    methodEntry.AddMember(Type::IntegerType(4, true), "types");
    methodEntry.AddMember(Type::IntegerType(4, true), "imp");
    type = FinalizeStructureBuilder(m_view, methodEntry, "objc_method_entry_t");
    m_types.MethodListEntry = type.first;

    StructureBuilder method;
    method.AddMember(Type::PointerType(addrSize, Type::VoidType()), "name");
    method.AddMember(Type::PointerType(addrSize, Type::VoidType()), "types");
    method.AddMember(Type::PointerType(addrSize, Type::VoidType()), "imp");
    type = FinalizeStructureBuilder(m_view, method, "objc_method_t");
    m_types.Method = type.first;

    StructureBuilder methList;
    methList.AddMember(Type::IntegerType(4, false), "obsolete");
    methList.AddMember(Type::IntegerType(4, false), "count");
    type = FinalizeStructureBuilder(m_view, methList, "objc_method_list_t");
    m_types.MethodList = type.first;

    StructureBuilder classROBuilder;
    classROBuilder.AddMember(Type::IntegerType(4, false), "flags");
    classROBuilder.AddMember(Type::IntegerType(4, false), "start");
    classROBuilder.AddMember(Type::IntegerType(4, false), "size");
    if (addrSize == 8)
        classROBuilder.AddMember(Type::IntegerType(4, false), "reserved");
    classROBuilder.AddMember(Type::NamedType(m_view, CustomTypes::TaggedPointer), "ivar_layout");
    classROBuilder.AddMember(Type::NamedType(m_view, CustomTypes::TaggedPointer), "name");
    classROBuilder.AddMember(Type::NamedType(m_view, CustomTypes::TaggedPointer), "methods");
    classROBuilder.AddMember(Type::NamedType(m_view, CustomTypes::TaggedPointer), "protocols");
    classROBuilder.AddMember(Type::NamedType(m_view, CustomTypes::TaggedPointer), "ivars");
    classROBuilder.AddMember(Type::NamedType(m_view, CustomTypes::TaggedPointer), "weak_ivar_layout");
    classROBuilder.AddMember(Type::NamedType(m_view, CustomTypes::TaggedPointer), "properties");
    type = FinalizeStructureBuilder(m_view, classROBuilder, "objc_class_ro_t");
    m_types.ClassRO = type.first;

    StructureBuilder classBuilder;
    classBuilder.AddMember(Type::NamedType(m_view, CustomTypes::TaggedPointer), "isa");
    classBuilder.AddMember(Type::NamedType(m_view, CustomTypes::TaggedPointer), "super");
    classBuilder.AddMember(Type::NamedType(m_view, CustomTypes::TaggedPointer), "cache");
    classBuilder.AddMember(Type::NamedType(m_view, CustomTypes::TaggedPointer), "vtable");
    classBuilder.AddMember(Type::NamedType(m_view, CustomTypes::TaggedPointer), "data");
    type = FinalizeStructureBuilder(m_view, classBuilder, "objc_class_t");
    m_types.Class = type.first;

    StructureBuilder ivarBuilder;
    ivarBuilder.AddMember(Type::PointerType(addrSize, Type::IntegerType(4, false)), "offset");
    ivarBuilder.AddMember(Type::PointerType(addrSize, Type::VoidType()), "name");
    ivarBuilder.AddMember(Type::PointerType(addrSize, Type::VoidType()), "type");
    ivarBuilder.AddMember(Type::IntegerType(4, false), "alignment");
    ivarBuilder.AddMember(Type::IntegerType(4, false), "size");
    type = FinalizeStructureBuilder(m_view, ivarBuilder, "objc_ivar_t");
    // m_types. = type.first;

    StructureBuilder ivarList;
    ivarList.AddMember(Type::IntegerType(4, false), "entsize");
    ivarList.AddMember(Type::IntegerType(4, false), "count");
    type = FinalizeStructureBuilder(m_view, ivarList, "objc_ivar_list_t");

    m_typesLoaded = true;
}


DSCFileView::DSCFileView(const std::string &typeName, BinaryView *data, bool parseOnly) : BinaryView(typeName,
                                                                                                     data->GetFile(),
                                                                                                     data) {
    m_filename = data->GetFile()->GetFilename();

    if (m_filename.empty()) {
        BNLogAlert("FileMetadata->GetFilename() returned null, something is terribly wrong");
        throw DSCFormatException();
    }

    auto fa = new MMappedFileAccessor(m_filename);

    m_cache = SharedCache::LoadCache(fa);

    m_length = UINT64_MAX;
}

size_t DSCFileView::PerformRead(void *dest, uint64_t offset, size_t len) {
    if (offset >= m_length)
        return 0;

    try {
        m_cache->m_vm->Read(dest, offset, len);
        return len;
    } catch (...) {
        return 0;
    }

}

bool DSCFileView::PerformIsOffsetBackedByFile(uint64_t offset) {
    return offset <= m_length;
}

uint64_t DSCFileView::PerformGetLength() const {
    return m_length;
}


bool has_suffix(const std::string &str, const std::string &suffix) {
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}


DSCView::DSCView(const std::string &typeName, DSCFileView *data, bool parseOnly) : BinaryView(typeName, data->GetFile(),
                                                                                              data),
                                                                                   m_parseOnly(parseOnly) {
    m_filename = data->GetFile()->GetFilename();
    m_dscFileView = data;
    m_logger = IssueSessionLogger("view");

    g_dscViews[data->GetFile()->GetSessionId()] = this;

    if (m_dscFileView)
        m_cache = m_dscFileView->m_cache;
    else {
        BNLogAlert("bad parent");
        throw DSCFormatException();
    }

    m_metrics = new Metrics();
    m_objcProcessor = new ObjCProcessing(this, m_cache);

    if (m_filename.empty()) {
        BNLogAlert("FileMetadata->GetFilename() returned null, something is terribly wrong");
        throw DSCFormatException();
    }

    if (has_suffix(m_filename, "arm64") || has_suffix(m_filename, "arm64e")) {
        m_arch = Architecture::GetByName("aarch64");
        SetDefaultArchitecture(Architecture::GetByName("aarch64"));
        SetDefaultPlatform(Platform::GetByName("mac-aarch64"));
        m_addressSize = 8;
    } else if (has_suffix(m_filename, "armv7") || has_suffix(m_filename, "armv7k")) {
        m_arch = Architecture::GetByName("armv7");
        SetDefaultArchitecture(Architecture::GetByName("armv7"));
        SetDefaultPlatform(Platform::GetByName("mac-armv7"));
        m_addressSize = 4;
    }

    m_pointerMask = 0x01FFFFFFFF;
}


bool DSCView::Init() {
    VMReader *reader = GetReader();
    size_t count = 0;
    for (auto const &[iName, cacheImage]: m_cache->m_images) {
        count++;
        reader->Seek(cacheImage.headerAddress);
        auto sects = MinLoader::SectionsInHeader(reader);
        for (auto s: sects) {
            m_addressRanges.push_back({
                                              .start = s.start,
                                              .len = s.length,
                                              .end = s.start + s.length,
                                              .installName = iName
                                      });
            //// BNLogInfo("%s: %llx - %llx", iName.c_str(), s.start, s.start + s.length);
        }
    }

    for (auto range : m_cache->m_dyldDataSections)
    {
        // BNLogInfo("Mapping in 0x%zx", range.start);
        AddAutoSegment(range.start, range.len,
                       range.start, range.len, SegmentExecutable | SegmentReadable);
    }

    LoadImageViaInstallName("/usr/lib/system/libsystem_platform.dylib");

    return true;
}

DSCView::~DSCView() {
    BNLogError("fucking dead");
}

Ref<Logger> DSCView::IssueSessionLogger(std::string component) {

    // For debug builds, we modularize the logging for the different components in the plugin
    // For end user builds, they are all done under the same logger.
#ifndef NDEBUG
    std::string loggerName = std::string(LogPrefix) + "." + component;
#else
    std::string loggerName = std::string(LogPrefix);
#endif
    return CreateLogger(loggerName);
}


void DSCView::LoadImageContainingAddress(uint64_t address) {
    for (const auto &addrRange: m_addressRanges) {
        if (address > addrRange.start) {
            if (address < addrRange.end) {
                LoadImageViaInstallName(addrRange.installName);
                return;
            }
        }
    }

    if (m_cache->m_vm->AddressIsMapped(address)) {
        // BNLogInfo("No Image but Address Is mapped, loading entire movie");
        for (const auto &addrRange: m_cache->m_mappedRanges) {
            if (address > addrRange.start) {
                if (address < addrRange.end) {
                    AddAutoSegment(addrRange.start, addrRange.len, addrRange.start, addrRange.len,
                                   SegmentExecutable | SegmentReadable);
                    LoadStubsSection(addrRange.start, addrRange.end);
                    return;
                }
            }
        }
    }

    BNLogWarn("No Image containing %llx", address);
}


void DSCView::LoadStubsSection(uint64_t start, uint64_t end) {
    UpdateAnalysisAndWait();

    for (auto func: GetAnalysisFunctionList()) {
        if (func->GetStart() > start && func->GetStart() < end) {
            if (func->GetHighLevelIL()) {
                auto hlilTail = func->GetHighLevelIL()->GetInstruction(0);
                if (hlilTail.operation == HLIL_TAILCALL) {
                    auto destExpr = hlilTail.GetDestExpr();
                    if (destExpr.operation == HLIL_CONST_PTR) {
                        auto addr = destExpr.GetConstant();
                        auto targetFuncs = GetAnalysisFunctionsForAddress(addr);
                        if (!targetFuncs.empty()) {
                            auto target = targetFuncs.at(0);
                            auto targetSym = target->GetSymbol();
                            // BNLogInfo("0x%llx: %s", func->GetStart(), targetSym->GetFullName().c_str());

                            DefineMachoSymbol(FunctionSymbol, targetSym->GetFullName(), func->GetStart(), LocalBinding,
                                              false);
                        }
                    }
                }
            }
        }
    }
}


void DSCView::LoadImageViaInstallName(std::string installName, bool isBaseName) {
    try {
        m_metrics = new Metrics();
        auto timer = GetMetrics()->AddMetric("Image Load");
        timer->StartTimer();

        std::pair<CacheImage, std::vector<LoadedImageBuffer>> imageSections;

        if (isBaseName)
            imageSections = m_cache->LoadImageByBaseName(installName);
        else
            imageSections = m_cache->LoadImageByInstallName(installName);

        for (auto s: imageSections.second) {
            if ((s.initprot == MACHO_VM_PROT_NONE) || (!s.data->GetLength()))
                continue;

            uint32_t flags = 0;
            if (s.initprot & MACHO_VM_PROT_READ)
                flags |= SegmentReadable;
            if (s.initprot & MACHO_VM_PROT_WRITE)
                flags |= SegmentWritable;
            if (s.initprot & MACHO_VM_PROT_EXECUTE)
                flags |= SegmentExecutable;

            // BNLogInfo("Loaded segment @ %llx w/ length %llx", s.address, s.data->GetLength());

            AddAutoSegment(s.address, s.data->GetLength(), s.address, s.data->GetLength(), flags);
        }
        size_t address = imageSections.first.headerAddress;

        timer->EndTimer();

        auto image = ProcessImageAtAddress(installName, address);

        if (image.is64)
            m_objcProcessor->LoadObjCMetadata(image);

        for (auto metric: m_metrics->durationItems) {
            // BNLogInfo("%s took %dms", metric->itemTitle.c_str(), metric->Duration<std::chrono::milliseconds>());
        }

        for (auto s: imageSections.second) {
            if ((s.initprot == MACHO_VM_PROT_NONE) || (!s.data->GetLength()))
                continue;
            Write(s.address, s.data->GetData(), s.data->GetLength());
        }

    }
    catch (const std::exception &ex) {
        BNLogError("Failed to load image with %s", ex.what());
    } catch (const std::string &ex) {
        BNLogError("Failed to load image with %s", ex.c_str());
    } catch (...) {
        BNLogError("Failed to load image with an unknown error");
    }
}


MachOImage DSCView::ProcessImageAtAddress(std::string installName, size_t address) {
    auto timer = GetMetrics()->AddMetric("MachO Processing");
    timer->StartTimer();

    if (!m_typesLoaded)
        LoadMachoTypes();

    VMReader *reader = GetReader();
    MachOImage image{};

    image.baseAddress = address;

    mach_header_64 header{};
    // BNLogInfo("Loading %s from 0x%zx", installName.c_str(), address);

    header.magic = reader->ReadUInt32(address);
    bool is64 = (header.magic == MH_MAGIC_64 || header.magic == MH_CIGAM_64);
    image.is64 = is64;
    header.cputype = reader->ReadInt32();
    header.cpusubtype = reader->ReadInt32();
    header.filetype = reader->ReadUInt32();
    header.ncmds = reader->ReadUInt32();
    header.sizeofcmds = reader->ReadUInt32();
    header.flags = reader->ReadUInt32();

    // This is an arbitrary number we're using as a sanity check.
    if (header.ncmds > 0xFFFF) {
        BNLogAlert("cmd count indicates likely bad address");
    }

    if (is64)
        DefineDataVariable(address, Type::NamedType(this, m_types.HeaderType));
    else
        DefineDataVariable(address, Type::NamedType(this, m_types.Header32Type));

    DefineAutoSymbol(new Symbol(DataSymbol, "__macho_header", address, LocalBinding));

    try {
        // TODO: this whole function has a lot of complex interplay with a "cursor" variable
        //      that existed before my VM reader had proper cursor support;
        //      so this is going to be unreadable to anyone else and I need to clean it up.
        size_t loadCommandOffset = is64 ? 32 : 28;
        size_t imageBase = address;
        size_t cursor;
        cursor = imageBase + loadCommandOffset;
        size_t sectionNum = 0;
        for (size_t i = 0; i < header.ncmds; i++) {
            load_command load{};
            uint64_t curOffset = cursor;
            load.cmd = reader->ReadUInt32(cursor);
            load.cmdsize = reader->ReadUInt32(cursor + 4);
            cursor += 8;
            uint64_t nextOffset = curOffset + load.cmdsize;
            switch (load.cmd) {
                case LC_SEGMENT: {
                    DefineDataVariable(curOffset, Type::NamedType(this, m_types.LoadSegmentCommandType));

                    segment_command seg{};
                    reader->Read(&seg, curOffset, sizeof(segment_command));
                    segment_command_64 equivalentSegment64 = {
                            .cmd = seg.cmd,
                            .cmdsize = seg.cmdsize,
                            .vmaddr = seg.vmaddr,
                            .vmsize = seg.vmsize,
                            .fileoff = seg.fileoff,
                            .filesize = seg.filesize,
                            .maxprot = seg.maxprot,
                            .initprot = seg.initprot,
                            .nsects = seg.nsects,
                            .flags = seg.flags,
                    };
                    memcpy(&equivalentSegment64.segname, &seg.segname, 16);

                    if (strncmp(seg.segname, "__LINKEDIT", 10) == 0)
                        image.m_linkeditSegment = equivalentSegment64;

                    image.m_segments.push_back(equivalentSegment64);

                    char segmentName[17];
                    strncpy(segmentName, seg.segname, 16);
                    segmentName[16] = 0;
                    // BNLogInfo("Seg: %s", segmentName);

                    size_t numSections = seg.nsects;
                    cursor += sizeof(segment_command) - 8;
                    for (size_t j = 0; j < numSections; j++) {
                        DefineDataVariable(cursor, Type::NamedType(this, m_types.SectionType));
                        DefineAutoSymbol(
                                new Symbol(DataSymbol, "__macho_section_[" + std::to_string(sectionNum++) + "]", cursor,
                                           LocalBinding));
                        section sect{};
                        reader->Read(&sect, cursor, sizeof(section));

                        section_64 equivalentSection64 = {
                                .addr = sect.addr,
                                .size = sect.size,
                                .offset = sect.offset,
                                .align = sect.align,
                                .reloff = sect.reloff,
                                .nreloc = sect.nreloc,
                                .flags = sect.flags
                        };
                        memcpy(&equivalentSection64.sectname, &sect.sectname, 16);
                        memcpy(&equivalentSection64.segname, &sect.segname, 16);

                        char sectName[17];
                        char segName[17];
                        strncpy(sectName, sect.sectname, 16);
                        sectName[16] = 0;
                        strncpy(segName, sect.segname, 16);
                        segName[16] = 0;
                        if (strncmp(sectName, "__objc", 6) == 0) {
                            image.hasObjCMetadata = true;
                        }
                        image.m_sections.push_back(equivalentSection64);
                        image.sectionsByName[sectName] = equivalentSection64;

                        std::string type;
                        BNSectionSemantics semantics = DefaultSectionSemantics;
                        switch (sect.flags & 0xff) {
                            case S_REGULAR:
                                if (sect.flags & S_ATTR_PURE_INSTRUCTIONS) {
                                    type = "PURE_CODE";
                                    semantics = ReadOnlyCodeSectionSemantics;
                                } else if (sect.flags & S_ATTR_SOME_INSTRUCTIONS) {
                                    type = "CODE";
                                    semantics = ReadOnlyCodeSectionSemantics;
                                } else {
                                    type = "REGULAR";
                                }
                                break;
                            case S_ZEROFILL:
                                type = "ZEROFILL";
                                semantics = ReadWriteDataSectionSemantics;
                                break;
                            case S_CSTRING_LITERALS:
                                type = "CSTRING_LITERALS";
                                semantics = ReadOnlyDataSectionSemantics;
                                break;
                            case S_4BYTE_LITERALS:
                                type = "4BYTE_LITERALS";
                                break;
                            case S_8BYTE_LITERALS:
                                type = "8BYTE_LITERALS";
                                break;
                            case S_LITERAL_POINTERS:
                                type = "LITERAL_POINTERS";
                                semantics = ReadOnlyDataSectionSemantics;
                                break;
                            case S_NON_LAZY_SYMBOL_POINTERS:
                                type = "NON_LAZY_SYMBOL_POINTERS";
                                semantics = ReadOnlyDataSectionSemantics;
                                break;
                            case S_LAZY_SYMBOL_POINTERS:
                                type = "LAZY_SYMBOL_POINTERS";
                                semantics = ReadOnlyDataSectionSemantics;
                                break;
                            case S_SYMBOL_STUBS:
                                type = "SYMBOL_STUBS";
                                semantics = ReadOnlyCodeSectionSemantics;
                                break;
                            case S_MOD_INIT_FUNC_POINTERS:
                                type = "MOD_INIT_FUNC_POINTERS";
                                semantics = ReadOnlyDataSectionSemantics;
                                break;
                            case S_MOD_TERM_FUNC_POINTERS:
                                type = "MOD_TERM_FUNC_POINTERS";
                                semantics = ReadOnlyDataSectionSemantics;
                                break;
                            case S_COALESCED:
                                type = "COALESCED";
                                break;
                            case S_GB_ZEROFILL:
                                type = "GB_ZEROFILL";
                                semantics = ReadWriteDataSectionSemantics;
                                break;
                            case S_INTERPOSING:
                                type = "INTERPOSING";
                                break;
                            case S_16BYTE_LITERALS:
                                type = "16BYTE_LITERALS";
                                break;
                            case S_DTRACE_DOF:
                                type = "DTRACE_DOF";
                                break;
                            case S_LAZY_DYLIB_SYMBOL_POINTERS:
                                type = "LAZY_DYLIB_SYMBOL_POINTERS";
                                semantics = ReadOnlyDataSectionSemantics;
                                break;
                            case S_THREAD_LOCAL_REGULAR:
                                type = "THREAD_LOCAL_REGULAR";
                                break;
                            case S_THREAD_LOCAL_ZEROFILL:
                                type = "THREAD_LOCAL_ZEROFILL";
                                break;
                            case S_THREAD_LOCAL_VARIABLES:
                                type = "THREAD_LOCAL_VARIABLES";
                                break;
                            case S_THREAD_LOCAL_VARIABLE_POINTERS:
                                type = "THREAD_LOCAL_VARIABLE_POINTERS";
                                break;
                            case S_THREAD_LOCAL_INIT_FUNCTION_POINTERS:
                                type = "THREAD_LOCAL_INIT_FUNCTION_POINTERS";
                                break;
                            default:
                                type = "UNKNOWN";
                                break;
                        }

                        if (strncmp(sectName, "__text", 16) == 0)
                            semantics = ReadOnlyCodeSectionSemantics;
                        if (strncmp(sectName, "__const", 16) == 0)
                            semantics = ReadOnlyDataSectionSemantics;
                        if (strncmp(sectName, "__data", 16) == 0)
                            semantics = ReadWriteDataSectionSemantics;
                        if (strncmp(sectName, "__DATA_CONST", 16) == 0)
                            semantics = ReadOnlyDataSectionSemantics;

                        AddAutoSection(fs::path(installName).filename().string() + "::" + sectName, sect.addr,
                                       sect.size,
                                       semantics, type);
                        cursor += sizeof(section);
                    }
                    break;
                }
                case LC_SEGMENT_64: {
                    DefineDataVariable(curOffset, Type::NamedType(this, m_types.LoadSegmentCommand64Type));
                    segment_command_64 seg{};
                    reader->Read(&seg, curOffset, sizeof(segment_command_64));

                    if (strncmp(seg.segname, "__LINKEDIT", 10) == 0)
                        image.m_linkeditSegment = seg;

                    image.m_segments.push_back(seg);

                    char segmentName[17];
                    strncpy(segmentName, seg.segname, 16);
                    segmentName[16] = 0;

                    cursor += (7 * 8);
                    size_t numSections = reader->ReadUInt32(cursor);
                    cursor += 8;
                    for (size_t j = 0; j < numSections; j++) {
                        DefineDataVariable(cursor, Type::NamedType(this, m_types.Section64Type));
                        DefineAutoSymbol(
                                new Symbol(DataSymbol, "__macho_section_64_[" + std::to_string(sectionNum++) + "]",
                                           cursor, LocalBinding));
                        section_64 sect{};
                        reader->Read(&sect, cursor, sizeof(section_64));
                        char sectName[17];
                        char segName[17];
                        strncpy(sectName, sect.sectname, 16);
                        sectName[16] = 0;
                        strncpy(segName, sect.segname, 16);
                        segName[16] = 0;
                        if (strncmp(sectName, "__objc", 6) == 0) {
                            image.hasObjCMetadata = true;
                        }
                        image.m_sections.push_back(sect);
                        image.sectionsByName[sectName] = sect;
                        size_t vaddr = reader->ReadULong(cursor + 32);
                        size_t size = reader->ReadULong(cursor + 40);

                        size_t flags = reader->ReadUInt32(cursor + 64);

                        std::string type;
                        BNSectionSemantics semantics = DefaultSectionSemantics;
                        switch (flags & 0xff) {
                            case S_REGULAR:
                                if (flags & S_ATTR_PURE_INSTRUCTIONS) {
                                    type = "PURE_CODE";
                                    semantics = ReadOnlyCodeSectionSemantics;
                                } else if (flags & S_ATTR_SOME_INSTRUCTIONS) {
                                    type = "CODE";
                                    semantics = ReadOnlyCodeSectionSemantics;
                                } else {
                                    type = "REGULAR";
                                }
                                break;
                            case S_ZEROFILL:
                                type = "ZEROFILL";
                                semantics = ReadWriteDataSectionSemantics;
                                break;
                            case S_CSTRING_LITERALS:
                                type = "CSTRING_LITERALS";
                                semantics = ReadOnlyDataSectionSemantics;
                                break;
                            case S_4BYTE_LITERALS:
                                type = "4BYTE_LITERALS";
                                break;
                            case S_8BYTE_LITERALS:
                                type = "8BYTE_LITERALS";
                                break;
                            case S_LITERAL_POINTERS:
                                type = "LITERAL_POINTERS";
                                semantics = ReadOnlyDataSectionSemantics;
                                break;
                            case S_NON_LAZY_SYMBOL_POINTERS:
                                type = "NON_LAZY_SYMBOL_POINTERS";
                                semantics = ReadOnlyDataSectionSemantics;
                                break;
                            case S_LAZY_SYMBOL_POINTERS:
                                type = "LAZY_SYMBOL_POINTERS";
                                semantics = ReadOnlyDataSectionSemantics;
                                break;
                            case S_SYMBOL_STUBS:
                                type = "SYMBOL_STUBS";
                                semantics = ReadOnlyCodeSectionSemantics;
                                break;
                            case S_MOD_INIT_FUNC_POINTERS:
                                type = "MOD_INIT_FUNC_POINTERS";
                                semantics = ReadOnlyDataSectionSemantics;
                                break;
                            case S_MOD_TERM_FUNC_POINTERS:
                                type = "MOD_TERM_FUNC_POINTERS";
                                semantics = ReadOnlyDataSectionSemantics;
                                break;
                            case S_COALESCED:
                                type = "COALESCED";
                                break;
                            case S_GB_ZEROFILL:
                                type = "GB_ZEROFILL";
                                semantics = ReadWriteDataSectionSemantics;
                                break;
                            case S_INTERPOSING:
                                type = "INTERPOSING";
                                break;
                            case S_16BYTE_LITERALS:
                                type = "16BYTE_LITERALS";
                                break;
                            case S_DTRACE_DOF:
                                type = "DTRACE_DOF";
                                break;
                            case S_LAZY_DYLIB_SYMBOL_POINTERS:
                                type = "LAZY_DYLIB_SYMBOL_POINTERS";
                                semantics = ReadOnlyDataSectionSemantics;
                                break;
                            case S_THREAD_LOCAL_REGULAR:
                                type = "THREAD_LOCAL_REGULAR";
                                break;
                            case S_THREAD_LOCAL_ZEROFILL:
                                type = "THREAD_LOCAL_ZEROFILL";
                                break;
                            case S_THREAD_LOCAL_VARIABLES:
                                type = "THREAD_LOCAL_VARIABLES";
                                break;
                            case S_THREAD_LOCAL_VARIABLE_POINTERS:
                                type = "THREAD_LOCAL_VARIABLE_POINTERS";
                                break;
                            case S_THREAD_LOCAL_INIT_FUNCTION_POINTERS:
                                type = "THREAD_LOCAL_INIT_FUNCTION_POINTERS";
                                break;
                            default:
                                type = "UNKNOWN";
                                break;
                        }

                        if (strncmp(sectName, "__text", 16) == 0)
                            semantics = ReadOnlyCodeSectionSemantics;
                        if (strncmp(sectName, "__const", 16) == 0)
                            semantics = ReadOnlyDataSectionSemantics;
                        if (strncmp(sectName, "__data", 16) == 0)
                            semantics = ReadWriteDataSectionSemantics;
                        if (strncmp(sectName, "__DATA_CONST", 16) == 0)
                            semantics = ReadOnlyDataSectionSemantics;

                        AddAutoSection(fs::path(installName).filename().string() + "::" + sectName, vaddr, size,
                                       semantics, type);
                        cursor += (10 * 8);
                    }
                    break;
                }
                case LC_SYMTAB:
                    DefineDataVariable(curOffset, Type::NamedType(this, m_types.SymTabType));
                    reader->Read(&image.symtabCmd, curOffset, sizeof(symtab_command));
                    break;
                case LC_DYSYMTAB:
                    DefineDataVariable(curOffset, Type::NamedType(this, m_types.DynSymTabType));
                    break;
                case LC_UUID:
                    DefineDataVariable(curOffset, Type::NamedType(this, m_types.UUIDType));
                    break;
                case LC_ID_DYLIB:
                    DefineDataVariable(curOffset, Type::NamedType(this, m_types.DylibCommandType));
                    if (load.cmdsize - 24 <= 150)
                        DefineDataVariable(curOffset + 24,
                                           Type::ArrayType(Type::IntegerType(1, true), load.cmdsize - 24));
                    break;
                case LC_LOAD_DYLIB:
                case LC_REEXPORT_DYLIB:
                case LC_LOAD_WEAK_DYLIB:
                case LC_LOAD_UPWARD_DYLIB: {
                    DefineDataVariable(curOffset, Type::NamedType(this, m_types.DylibCommandType));
                    if (load.cmdsize - 24 <= 150)
                        DefineDataVariable(curOffset + 24,
                                           Type::ArrayType(Type::IntegerType(1, true), load.cmdsize - 24));
                    auto result = reader->ReadBuffer(curOffset + 24, load.cmdsize - 24);
                    std::string depName = std::string((const char *) result->GetData(), load.cmdsize - 24);
                    depName.erase(std::find(depName.begin(), depName.end(), '\0'), depName.end());
                    image.m_deps.push_back(depName);
                    break;
                }
                case LC_CODE_SIGNATURE:
                case LC_SEGMENT_SPLIT_INFO:
                case LC_FUNCTION_STARTS:
                case LC_DATA_IN_CODE:
                case LC_DYLIB_CODE_SIGN_DRS:
                case LC_DYLD_CHAINED_FIXUPS:
                    DefineDataVariable(curOffset, Type::NamedType(this, m_types.LinkEditData));
                    break;
                case LC_DYLD_EXPORTS_TRIE:
                    DefineDataVariable(curOffset, Type::NamedType(this, m_types.LinkEditData));
                    reader->Read(&image.exportTrie, curOffset, sizeof(linkedit_data_command));
                    break;
                case LC_ENCRYPTION_INFO:
                    DefineDataVariable(curOffset, Type::NamedType(this, m_types.EncryptionInfoType));
                    break;
                case LC_VERSION_MIN_MACOSX:
                case LC_VERSION_MIN_IPHONEOS:
                    DefineDataVariable(curOffset, Type::NamedType(this, m_types.VersionMinType));
                    break;
                case LC_DYLD_INFO:
                case LC_DYLD_INFO_ONLY:
                    DefineDataVariable(curOffset, Type::NamedType(this, m_types.DyldInfoType));
                    reader->Read(&image.dyldInfo, curOffset, sizeof(dyld_info_command));
                    break;
                default:
                    DefineDataVariable(curOffset, Type::NamedType(this, m_types.LoadCommand));
                    break;
            }

            DefineAutoSymbol(
                    new Symbol(DataSymbol, "__macho_load_command_[" + std::to_string(i) + "]", curOffset,
                               LocalBinding));
            cursor = nextOffset;
        }
    } catch (ReadException &) {
        BNLogError("Error when applying Mach-O header types at %p", address);
    }

    LoadSymtab(image);
    LoadExportTrie(image);

    for (const auto &dep: image.m_deps) {
        // BNLogInfo("dep img = %s", dep.c_str());

        if (auto depImg = m_cache->m_images.find(dep); depImg != m_cache->m_images.end()) {
            auto minReader = GetReader();
            uint64_t addr = depImg->second.headerAddress;
            minReader->Seek(addr);
            auto exportInfo = MinLoader::ExportTrieAndLinkeditBaseForImage(minReader);
            LoadDepExports(addr, exportInfo.first, exportInfo.second);
            minReader->Seek(addr);
            auto symtabInfo = MinLoader::SymtabAndLinkeditBaseForImage(minReader);
            MachOImage fakeImg;
            fakeImg.symtabCmd = symtabInfo.first;
            fakeImg.m_linkeditSegment = symtabInfo.second;
            LoadSymtab(fakeImg);
        } else {
            BNLogWarn("Couldn't find dep %s in cache", dep.c_str());
#if DEPENDENCY_DEBUG
            for (auto i : m_cache->m_images)
            {
               // BNLogInfo("%s", i.first.c_str());
            }
#endif
        }
    }

    if (image.dyldInfo.bind_size)
        ParseBindingTable(image, image.dyldInfo.bind_off, image.dyldInfo.bind_size);
    if (image.dyldInfo.weak_bind_size)
        ParseBindingTable(image, image.dyldInfo.weak_bind_off, image.dyldInfo.weak_bind_size);
    if (image.dyldInfo.lazy_bind_size)
        ParseBindingTable(image, image.dyldInfo.lazy_bind_off, image.dyldInfo.lazy_bind_size);

    timer->EndTimer();
    return image;
}

void DSCView::ProcessSlideInfo()
{

}


void DSCView::LoadDepExports(uint64_t imageBase, linkedit_data_command cmd, uint64_t linkeditBase) {
    try {
        MMappedFileAccessor *reader = m_cache->m_vm->MappingAtAddress(linkeditBase).first.file;
        std::string startText = "";
        std::vector<ExportNode> nodes;
        DataBuffer* buffer = reader->ReadBuffer(cmd.dataoff, cmd.datasize);
        ReadExportNode(*buffer, nodes, startText, 0, cmd.datasize);

        for (const auto &n: nodes) {
            if (!n.text.empty() && n.offset) {
                uint32_t flags;
                BNSymbolType type = DataSymbol;

                // BNLogInfo("dep export: %s -> 0x%llx", n.text.c_str(), imageBase + n.offset);

                DefineMachoSymbol(type, n.text, imageBase + n.offset, GlobalBinding, false);
            }
        }
    }
    catch (...) {
        BNLogError("Failed to load dep Export Trie");
    }
}

std::vector<ExportTrieEntryStart> DSCView::ReadExportNode(DataBuffer& buffer, std::vector<ExportNode>& results, const std::string& currentText, size_t cursor, uint32_t endGuard)
{
    if (cursor > endGuard)
        throw ReadException();

    uint64_t terminalSize = readValidULEB128(buffer, cursor);
    if (terminalSize != 0) {
        uint64_t imageOffset = 0;
        uint64_t flags = readValidULEB128(buffer, cursor);
        if (!(flags & EXPORT_SYMBOL_FLAGS_REEXPORT))
        {
            imageOffset = readValidULEB128(buffer, cursor);
            results.push_back({currentText, imageOffset, flags});
        }
    }
    uint8_t childCount = buffer[cursor];
    cursor++;
    if (cursor > endGuard)
        throw ReadException();

    std::vector<ExportTrieEntryStart> entries;
    for (uint8_t i = 0; i < childCount; ++i)
    {
        std::string childText;
        while (buffer[cursor] != 0 & cursor <= endGuard)
            childText.push_back(buffer[cursor++]);
        cursor++;
        if (cursor > endGuard)
            throw ReadException();
        auto next = readValidULEB128(buffer, cursor);
        if (next == 0)
            throw ReadException();
        entries.push_back({currentText + childText, next});
    }
    return entries;
}


void DSCView::LoadExportTrie(MachOImage &image) {
    try {
        MMappedFileAccessor *reader = m_cache->m_vm->MappingAtAddress(image.m_linkeditSegment.vmaddr).first.file;

        std::string startText = "";
        std::vector<ExportNode> nodes;
        DataBuffer* buffer = reader->ReadBuffer(image.exportTrie.dataoff, image.exportTrie.datasize);
        std::deque<ExportTrieEntryStart> entries;
        entries.push_back({"", 0});
        while (!entries.empty())
        {
            for (std::deque<ExportTrieEntryStart>::iterator it = entries.begin(); it != entries.end();)
            {
                ExportTrieEntryStart entry = *it;
                it = entries.erase(it);

                for (const auto& newEntry : ReadExportNode(*buffer, nodes, entry.currentText,
                                                           entry.cursorPosition, image.exportTrie.datasize))
                    entries.push_back(newEntry);
            }
        }

        for (const auto &n: nodes) {
            if (!n.text.empty() && n.offset) {
                uint32_t flags;
                BNSymbolType type = DataSymbol;
                auto found = false;
                for (auto s: image.m_sections) {
                    if (s.addr < n.offset) {
                        if (s.addr + s.size > n.offset) {
                            flags = s.flags;
                            found = true;
                        }
                    }
                }
                if ((flags & S_ATTR_PURE_INSTRUCTIONS) == S_ATTR_PURE_INSTRUCTIONS ||
                    (flags & S_ATTR_SOME_INSTRUCTIONS) == S_ATTR_SOME_INSTRUCTIONS)
                    type = FunctionSymbol;
                else
                    type = DataSymbol;
#if EXPORT_TRIE_DEBUG
                // BNLogInfo("export: %s -> 0x%llx", n.text.c_str(), image.baseAddress + n.offset);
#endif
                DefineMachoSymbol(type, n.text, image.baseAddress + n.offset, NoBinding, false);
            }
        }
    } catch (std::exception &e) {
        BNLogError("Failed to load Export Trie");
    }
}


void DSCView::LoadSymtab(MachOImage &image) {
    // All calcs here are based off of fileoffset, which can be any arbitrary one;
    //      LINKEDIT seg vm addr will map to the proper file.

    size_t stringTableBase = image.symtabCmd.stroff;
    size_t nlistBase = image.symtabCmd.symoff;

    MMappedFileAccessor *file = m_cache->m_vm->MappingAtAddress(image.m_linkeditSegment.vmaddr).first.file;

    nlist_64 sym{};
    nlist sym32{};

    for (size_t i = 0; i < image.symtabCmd.nsyms; i++) {
        if (image.is64) {
            size_t off = nlistBase + (i * sizeof(nlist_64));
            file->Read(&sym, off, sizeof(nlist_64));
        } else {
            size_t off = nlistBase + (i * sizeof(nlist));
            file->Read(&sym32, off, sizeof(nlist));
            sym = {
                    .n_strx = static_cast<uint32_t>(sym32.n_strx),
                    .n_type = sym32.n_type,
                    .n_sect = sym32.n_sect,
                    .n_desc = static_cast<uint16_t>(sym32.n_desc),
                    .n_value = sym32.n_value
            };
        }
        if ((sym.n_desc & N_ARM_THUMB_DEF) == N_ARM_THUMB_DEF)
            sym.n_value++;
        std::string name = file->ReadNullTermString(sym.n_strx + stringTableBase);
        if (name == "<redacted>")
            continue;
        if (sym.n_value && !name.empty()) {
            uint32_t flags;
            BNSymbolType type = DataSymbol;
            for (auto s: image.m_sections) {
                if (s.addr < sym.n_value) {
                    if (s.addr + s.size > sym.n_value)
                        flags = s.flags;
                }
            }
            if ((flags & S_ATTR_PURE_INSTRUCTIONS) == S_ATTR_PURE_INSTRUCTIONS ||
                (flags & S_ATTR_SOME_INSTRUCTIONS) == S_ATTR_SOME_INSTRUCTIONS)
                type = FunctionSymbol;
            else
                type = DataSymbol;
            if (image.m_sections.empty())
                // BNLogInfo("symtab: %s -> 0x%llx", name.c_str(), sym.n_value);
                DefineMachoSymbol(type, name, sym.n_value, NoBinding, false);
        }
    }

}


void DSCView::ParseBindingTable(MachOImage &image, size_t table_start, size_t tableSize) {
    // This whole function is implemented differently than in Mach-O View because
    //      machoview.cpp is unreadable if you dont know the mach-o spec well. ._.

    MMappedFileAccessor *reader = m_cache->m_vm->MappingAtAddress(image.m_linkeditSegment.vmaddr).first.file;

    size_t cursor = table_start;
    size_t tableEnd = table_start + tableSize;

    std::vector<BindingRecord> importStack;

    bool usesThreadedBind = false;

    BindingRecord currentRecord{};

    currentRecord.seg_index = 0;
    currentRecord.seg_offset = 0;
    currentRecord.lib_ordinal = 0;
    currentRecord.type = 0;
    currentRecord.flags = 0;
    currentRecord.name = "";
    currentRecord.addend = 0;
    currentRecord.special_dylib = 0;

    bool done = false;

    while (cursor < tableEnd) {
        uint8_t opcode = reader->ReadUChar(cursor) & 0xF0;
        uint8_t value = reader->ReadUChar(cursor) & 0x0F;

        size_t cmdStart = cursor;
        cursor++;

        switch (opcode) {
            case DONE:
                currentRecord.seg_index = 0;
                currentRecord.seg_offset = 0;
                currentRecord.name = "";
                currentRecord.type = 0;
                break;
            case SET_DYLIB_ORDINAL_IMM:
                currentRecord.lib_ordinal = value;
                break;
            case SET_DYLIB_ORDINAL_ULEB:
                currentRecord.lib_ordinal = ReadULEB128(reader, cursor, tableEnd);
                break;
            case SET_DYLIB_SPECIAL_IMM:
                currentRecord.special_dylib = 1;
                currentRecord.lib_ordinal = value;
                break;
            case SET_SYMBOL_TRAILING_FLAGS_IMM:
                currentRecord.flags = value;
                currentRecord.name = reader->ReadNullTermString(cursor);
                cursor += currentRecord.name.length() + 1;
                break;
            case SET_TYPE_IMM:
                currentRecord.type = value;
                break;
            case SET_ADDEND_SLEB:
                currentRecord.addend = ReadSLEB128(reader, cursor, tableEnd);
                break;
            case SET_SEGMENT_AND_OFFSET_ULEB:
                currentRecord.seg_index = value;
                currentRecord.seg_offset = ReadULEB128(reader, cursor, tableEnd);
                break;
            case ADD_ADDR_ULEB:
                currentRecord.seg_offset += ReadULEB128(reader, cursor, tableEnd);
                break;
            case DO_BIND:
                importStack.push_back(currentRecord);
                currentRecord.seg_offset += m_addressSize;
            case DO_BIND_ADD_ADDR_ULEB:
                importStack.push_back(currentRecord);
                currentRecord.seg_offset += m_addressSize;
                currentRecord.seg_offset += ReadULEB128(reader, cursor, tableEnd);
                break;
            case DO_BIND_ADD_ADDR_IMM_SCALED:
                importStack.push_back(currentRecord);
                currentRecord.seg_offset += m_addressSize;
                currentRecord.seg_offset += value * m_addressSize;
                break;
            case DO_BIND_ULEB_TIMES_SKIPPING_ULEB: {
                size_t count = ReadULEB128(reader, cursor, tableEnd);
                size_t skip = ReadULEB128(reader, cursor, tableEnd);
                for (size_t i = 0; i < count; i++) {
                    currentRecord.seg_offset += m_addressSize + skip;
                    importStack.push_back(currentRecord);
                }
                break;
            }
            case THREADED:
                if (value == BIND_SUBOPCODE_THREADED_SET_BIND_ORDINAL_TABLE_SIZE_ULEB) {
                    size_t _ = ReadULEB128(reader, cursor, tableEnd);
                    usesThreadedBind = true;
                }
                break;
            default:
                break;
        }
    }

    for (BindingRecord &record: importStack) {
        size_t address = image.m_segments.at(record.seg_index).vmaddr;
        address += record.seg_offset;
        // if (record.seg_index < image.m_segments.size())
        //// BNLogInfo("binding: n: %s -> 0x%zx", record.name.c_str(),
        //                image.m_segments.at(record.seg_index).vmaddr + address);
    }
}


Ref<Symbol>
DSCView::DefineMachoSymbol(BNSymbolType type, const std::string &name, uint64_t addr, BNSymbolBinding binding,
                           bool deferred) {
    Ref<Type> symbolTypeRef;

    // If name is empty, symbol is not valid
    if (name.empty())
        return nullptr;

    if (type != ExternalSymbol) {

    }

    auto process = [=]() {
        // If name does not start with alphabetic character or symbol, prepend an underscore
        std::string rawName = name;
        if (!(((name[0] >= 'A') && (name[0] <= 'Z')) || ((name[0] >= 'a') && (name[0] <= 'z')) || (name[0] == '_') ||
              (name[0] == '?') || (name[0] == '$') || (name[0] == '@')))
            rawName = "_" + name;

        NameSpace nameSpace = DSCView::GetInternalNameSpace();
        if (type == ExternalSymbol) {
            nameSpace = DSCView::GetExternalNameSpace();
        }

        // Try to demangle any C++ symbols
        std::string shortName = rawName;
        std::string fullName = rawName;
        Ref<Type> typeRef = symbolTypeRef;

        QualifiedName varName;
        if (m_arch) {
            if (IsGNU3MangledString(rawName)) {
                Ref<Type> demangledType;
                if (DemangleGNU3(m_arch, rawName, demangledType, varName, this)) {
                    shortName = varName.GetString();
                    fullName = shortName;
                    if (demangledType)
                        fullName += demangledType->GetStringAfterName();
                    if (!typeRef && !GetDefaultPlatform()->GetFunctionByName(rawName))
                        typeRef = demangledType;
                } else {
                    LogDebug("Failed to demangle name: '%s'\n", rawName.c_str());
                }
            }
        }

        return std::pair<Symbol *, Ref<Type>>(new Symbol(type, shortName, fullName, rawName, addr, binding, nameSpace),
                                              typeRef);
    };

    auto result = process();
    return DefineAutoSymbolAndVariableOrFunction(GetDefaultPlatform(), result.first, result.second);
}


void DSCView::LoadMachoTypes() {
    std::unique_lock<std::mutex> lock(m_typeDefMutex);

    if (m_typesLoaded)
        return;

    EnumerationBuilder cpuTypeBuilder;
    cpuTypeBuilder.AddMemberWithValue("CPU_TYPE_ANY", MACHO_CPU_TYPE_ANY);
    cpuTypeBuilder.AddMemberWithValue("CPU_TYPE_VAX", MACHO_CPU_TYPE_VAX);
    cpuTypeBuilder.AddMemberWithValue("CPU_TYPE_MC680x0", MACHO_CPU_TYPE_MC680x0);
    cpuTypeBuilder.AddMemberWithValue("CPU_TYPE_X86", MACHO_CPU_TYPE_X86);
    cpuTypeBuilder.AddMemberWithValue("CPU_TYPE_X86_64", MACHO_CPU_TYPE_X86_64);
    cpuTypeBuilder.AddMemberWithValue("CPU_TYPE_MIPS", MACHO_CPU_TYPE_MIPS);
    cpuTypeBuilder.AddMemberWithValue("CPU_TYPE_MC98000", MACHO_CPU_TYPE_MC98000);
    cpuTypeBuilder.AddMemberWithValue("CPU_TYPE_HPPA", MACHO_CPU_TYPE_HPPA);
    cpuTypeBuilder.AddMemberWithValue("CPU_TYPE_ARM", MACHO_CPU_TYPE_ARM);
    cpuTypeBuilder.AddMemberWithValue("CPU_TYPE_ARM64", MACHO_CPU_TYPE_ARM64);
    cpuTypeBuilder.AddMemberWithValue("CPU_TYPE_ARM64_32", MACHO_CPU_TYPE_ARM64_32);
    cpuTypeBuilder.AddMemberWithValue("CPU_TYPE_MC88000", MACHO_CPU_TYPE_MC88000);
    cpuTypeBuilder.AddMemberWithValue("CPU_TYPE_SPARC", MACHO_CPU_TYPE_SPARC);
    cpuTypeBuilder.AddMemberWithValue("CPU_TYPE_I860", MACHO_CPU_TYPE_I860);
    cpuTypeBuilder.AddMemberWithValue("CPU_TYPE_ALPHA", MACHO_CPU_TYPE_ALPHA);
    cpuTypeBuilder.AddMemberWithValue("CPU_TYPE_POWERPC", MACHO_CPU_TYPE_POWERPC);
    cpuTypeBuilder.AddMemberWithValue("CPU_TYPE_POWERPC64", MACHO_CPU_TYPE_POWERPC64);
    Ref<Enumeration> cpuTypeEnum = cpuTypeBuilder.Finalize();

    Ref<Type> cpuTypeEnumType = Type::EnumerationType(GetDefaultArchitecture(), cpuTypeEnum, 4, false);
    std::string cpuTypeEnumName = "cpu_type_t";
    std::string cpuTypeEnumId = Type::GenerateAutoTypeId("macho", cpuTypeEnumName);
    m_types.CPUTypeEnum = DefineType(cpuTypeEnumId, cpuTypeEnumName, cpuTypeEnumType);

    EnumerationBuilder fileTypeBuilder;
    fileTypeBuilder.AddMemberWithValue("MH_OBJECT", MH_OBJECT);
    fileTypeBuilder.AddMemberWithValue("MH_EXECUTE", MH_EXECUTE);
    fileTypeBuilder.AddMemberWithValue("MH_FVMLIB", MH_FVMLIB);
    fileTypeBuilder.AddMemberWithValue("MH_CORE", MH_CORE);
    fileTypeBuilder.AddMemberWithValue("MH_PRELOAD", MH_PRELOAD);
    fileTypeBuilder.AddMemberWithValue("MH_DYLIB", MH_DYLIB);
    fileTypeBuilder.AddMemberWithValue("MH_DYLINKER", MH_DYLINKER);
    fileTypeBuilder.AddMemberWithValue("MH_BUNDLE", MH_BUNDLE);
    fileTypeBuilder.AddMemberWithValue("MH_DYLIB_STUB", MH_DYLIB_STUB);
    fileTypeBuilder.AddMemberWithValue("MH_DSYM", MH_DSYM);
    fileTypeBuilder.AddMemberWithValue("MH_KEXT_BUNDLE", MH_KEXT_BUNDLE);
    Ref<Enumeration> fileTypeEnum = fileTypeBuilder.Finalize();

    Ref<Type> fileTypeEnumType = Type::EnumerationType(GetDefaultArchitecture(), fileTypeEnum, 4, false);
    std::string fileTypeEnumName = "file_type_t";
    std::string fileTypeEnumId = Type::GenerateAutoTypeId("macho", fileTypeEnumName);
    m_types.FileTypeEnum = DefineType(fileTypeEnumId, fileTypeEnumName, fileTypeEnumType);

    EnumerationBuilder flagsTypeBuilder;
    flagsTypeBuilder.AddMemberWithValue("MH_NOUNDEFS", MH_NOUNDEFS);
    flagsTypeBuilder.AddMemberWithValue("MH_INCRLINK", MH_INCRLINK);
    flagsTypeBuilder.AddMemberWithValue("MH_DYLDLINK", MH_DYLDLINK);
    flagsTypeBuilder.AddMemberWithValue("MH_BINDATLOAD", MH_BINDATLOAD);
    flagsTypeBuilder.AddMemberWithValue("MH_PREBOUND", MH_PREBOUND);
    flagsTypeBuilder.AddMemberWithValue("MH_SPLIT_SEGS", MH_SPLIT_SEGS);
    flagsTypeBuilder.AddMemberWithValue("MH_LAZY_INIT", MH_LAZY_INIT);
    flagsTypeBuilder.AddMemberWithValue("MH_TWOLEVEL", MH_TWOLEVEL);
    flagsTypeBuilder.AddMemberWithValue("MH_FORCE_FLAT", MH_FORCE_FLAT);
    flagsTypeBuilder.AddMemberWithValue("MH_NOMULTIDEFS", MH_NOMULTIDEFS);
    flagsTypeBuilder.AddMemberWithValue("MH_NOFIXPREBINDING", MH_NOFIXPREBINDING);
    flagsTypeBuilder.AddMemberWithValue("MH_PREBINDABLE", MH_PREBINDABLE);
    flagsTypeBuilder.AddMemberWithValue("MH_ALLMODSBOUND", MH_ALLMODSBOUND);
    flagsTypeBuilder.AddMemberWithValue("MH_SUBSECTIONS_VIA_SYMBOLS", MH_SUBSECTIONS_VIA_SYMBOLS);
    flagsTypeBuilder.AddMemberWithValue("MH_CANONICAL", MH_CANONICAL);
    flagsTypeBuilder.AddMemberWithValue("MH_WEAK_DEFINES", MH_WEAK_DEFINES);
    flagsTypeBuilder.AddMemberWithValue("MH_BINDS_TO_WEAK", MH_BINDS_TO_WEAK);
    flagsTypeBuilder.AddMemberWithValue("MH_ALLOW_STACK_EXECUTION", MH_ALLOW_STACK_EXECUTION);
    flagsTypeBuilder.AddMemberWithValue("MH_ROOT_SAFE", MH_ROOT_SAFE);
    flagsTypeBuilder.AddMemberWithValue("MH_SETUID_SAFE", MH_SETUID_SAFE);
    flagsTypeBuilder.AddMemberWithValue("MH_NO_REEXPORTED_DYLIBS", MH_NO_REEXPORTED_DYLIBS);
    flagsTypeBuilder.AddMemberWithValue("MH_PIE", MH_PIE);
    flagsTypeBuilder.AddMemberWithValue("MH_DEAD_STRIPPABLE_DYLIB", MH_DEAD_STRIPPABLE_DYLIB);
    flagsTypeBuilder.AddMemberWithValue("MH_HAS_TLV_DESCRIPTORS", MH_HAS_TLV_DESCRIPTORS);
    flagsTypeBuilder.AddMemberWithValue("MH_NO_HEAP_EXECUTION", MH_NO_HEAP_EXECUTION);
    flagsTypeBuilder.AddMemberWithValue("MH_APP_EXTENSION_SAFE", _MH_APP_EXTENSION_SAFE);
    Ref<Enumeration> flagsTypeEnum = flagsTypeBuilder.Finalize();

    Ref<Type> flagsTypeEnumType = Type::EnumerationType(GetDefaultArchitecture(), flagsTypeEnum, 4, false);
    std::string flagsTypeEnumName = "flags_type_t";
    std::string flagsTypeEnumId = Type::GenerateAutoTypeId("macho", flagsTypeEnumName);
    m_types.FlagsTypeEnum = DefineType(flagsTypeEnumId, flagsTypeEnumName, flagsTypeEnumType);

    StructureBuilder machoHeader32Builder;
    machoHeader32Builder.AddMember(Type::IntegerType(4, false), "magic");
    machoHeader32Builder.AddMember(Type::NamedType(this, m_types.CPUTypeEnum), "cputype");
    machoHeader32Builder.AddMember(Type::IntegerType(4, false), "cpusubtype");
    machoHeader32Builder.AddMember(Type::NamedType(this, m_types.FileTypeEnum), "filetype");
    machoHeader32Builder.AddMember(Type::IntegerType(4, false), "ncmds");
    machoHeader32Builder.AddMember(Type::IntegerType(4, false), "sizeofcmds");
    machoHeader32Builder.AddMember(Type::NamedType(this, m_types.FlagsTypeEnum), "flags");
    Ref<Structure> machoHeader32Struct = machoHeader32Builder.Finalize();
    QualifiedName header32Name = std::string("mach_header");

    std::string header32TypeId = Type::GenerateAutoTypeId("macho", header32Name);
    Ref<Type> machoHeader32Type = Type::StructureType(machoHeader32Struct);
    m_types.Header32Type = DefineType(header32TypeId, header32Name, machoHeader32Type);

    StructureBuilder machoHeaderBuilder;
    machoHeaderBuilder.AddMember(Type::IntegerType(4, false), "magic");
    machoHeaderBuilder.AddMember(Type::NamedType(this, m_types.CPUTypeEnum), "cputype");
    machoHeaderBuilder.AddMember(Type::IntegerType(4, false), "cpusubtype");
    machoHeaderBuilder.AddMember(Type::NamedType(this, m_types.FileTypeEnum), "filetype");
    machoHeaderBuilder.AddMember(Type::IntegerType(4, false), "ncmds");
    machoHeaderBuilder.AddMember(Type::IntegerType(4, false), "sizeofcmds");
    machoHeaderBuilder.AddMember(Type::NamedType(this, m_types.FlagsTypeEnum), "flags");
    machoHeaderBuilder.AddMember(Type::IntegerType(4, false), "reserved");
    Ref<Structure> machoHeaderStruct = machoHeaderBuilder.Finalize();
    QualifiedName headerName = std::string("mach_header_64");

    std::string headerTypeId = Type::GenerateAutoTypeId("macho", headerName);
    Ref<Type> machoHeaderType = Type::StructureType(machoHeaderStruct);
    m_types.HeaderType = DefineType(headerTypeId, headerName, machoHeaderType);

    EnumerationBuilder cmdTypeBuilder;
    cmdTypeBuilder.AddMemberWithValue("LC_REQ_DYLD", LC_REQ_DYLD);
    cmdTypeBuilder.AddMemberWithValue("LC_SEGMENT", LC_SEGMENT);
    cmdTypeBuilder.AddMemberWithValue("LC_SYMTAB", LC_SYMTAB);
    cmdTypeBuilder.AddMemberWithValue("LC_SYMSEG", LC_SYMSEG);
    cmdTypeBuilder.AddMemberWithValue("LC_THREAD", LC_THREAD);
    cmdTypeBuilder.AddMemberWithValue("LC_UNIXTHREAD", LC_UNIXTHREAD);
    cmdTypeBuilder.AddMemberWithValue("LC_LOADFVMLIB", LC_LOADFVMLIB);
    cmdTypeBuilder.AddMemberWithValue("LC_IDFVMLIB", LC_IDFVMLIB);
    cmdTypeBuilder.AddMemberWithValue("LC_IDENT", LC_IDENT);
    cmdTypeBuilder.AddMemberWithValue("LC_FVMFILE", LC_FVMFILE);
    cmdTypeBuilder.AddMemberWithValue("LC_PREPAGE", LC_PREPAGE);
    cmdTypeBuilder.AddMemberWithValue("LC_DYSYMTAB", LC_DYSYMTAB);
    cmdTypeBuilder.AddMemberWithValue("LC_LOAD_DYLIB", LC_LOAD_DYLIB);
    cmdTypeBuilder.AddMemberWithValue("LC_ID_DYLIB", LC_ID_DYLIB);
    cmdTypeBuilder.AddMemberWithValue("LC_LOAD_DYLINKER", LC_LOAD_DYLINKER);
    cmdTypeBuilder.AddMemberWithValue("LC_ID_DYLINKER", LC_ID_DYLINKER);
    cmdTypeBuilder.AddMemberWithValue("LC_PREBOUND_DYLIB", LC_PREBOUND_DYLIB);
    cmdTypeBuilder.AddMemberWithValue("LC_ROUTINES", LC_ROUTINES);
    cmdTypeBuilder.AddMemberWithValue("LC_SUB_FRAMEWORK", LC_SUB_FRAMEWORK);
    cmdTypeBuilder.AddMemberWithValue("LC_SUB_UMBRELLA", LC_SUB_UMBRELLA);
    cmdTypeBuilder.AddMemberWithValue("LC_SUB_CLIENT", LC_SUB_CLIENT);
    cmdTypeBuilder.AddMemberWithValue("LC_SUB_LIBRARY", LC_SUB_LIBRARY);
    cmdTypeBuilder.AddMemberWithValue("LC_TWOLEVEL_HINTS", LC_TWOLEVEL_HINTS);
    cmdTypeBuilder.AddMemberWithValue("LC_PREBIND_CKSUM", LC_PREBIND_CKSUM);
    cmdTypeBuilder.AddMemberWithValue("LC_LOAD_WEAK_DYLIB", LC_LOAD_WEAK_DYLIB);//       (0x18 | LC_REQ_DYLD)
    cmdTypeBuilder.AddMemberWithValue("LC_SEGMENT_64", LC_SEGMENT_64);
    cmdTypeBuilder.AddMemberWithValue("LC_ROUTINES_64", LC_ROUTINES_64);
    cmdTypeBuilder.AddMemberWithValue("LC_UUID", LC_UUID);
    cmdTypeBuilder.AddMemberWithValue("LC_RPATH", LC_RPATH);//                 (0x1c | LC_REQ_DYLD)
    cmdTypeBuilder.AddMemberWithValue("LC_CODE_SIGNATURE", LC_CODE_SIGNATURE);
    cmdTypeBuilder.AddMemberWithValue("LC_SEGMENT_SPLIT_INFO", LC_SEGMENT_SPLIT_INFO);
    cmdTypeBuilder.AddMemberWithValue("LC_REEXPORT_DYLIB", LC_REEXPORT_DYLIB);//        (0x1f | LC_REQ_DYLD)
    cmdTypeBuilder.AddMemberWithValue("LC_LAZY_LOAD_DYLIB", LC_LAZY_LOAD_DYLIB);
    cmdTypeBuilder.AddMemberWithValue("LC_ENCRYPTION_INFO", LC_ENCRYPTION_INFO);
    cmdTypeBuilder.AddMemberWithValue("LC_DYLD_INFO", LC_DYLD_INFO);
    cmdTypeBuilder.AddMemberWithValue("LC_DYLD_INFO_ONLY", LC_DYLD_INFO_ONLY);//        (0x22 | LC_REQ_DYLD)
    cmdTypeBuilder.AddMemberWithValue("LC_LOAD_UPWARD_DYLIB", LC_LOAD_UPWARD_DYLIB);//     (0x23 | LC_REQ_DYLD)
    cmdTypeBuilder.AddMemberWithValue("LC_VERSION_MIN_MACOSX", LC_VERSION_MIN_MACOSX);
    cmdTypeBuilder.AddMemberWithValue("LC_VERSION_MIN_IPHONEOS", LC_VERSION_MIN_IPHONEOS);
    cmdTypeBuilder.AddMemberWithValue("LC_FUNCTION_STARTS", LC_FUNCTION_STARTS);
    cmdTypeBuilder.AddMemberWithValue("LC_DYLD_ENVIRONMENT", LC_DYLD_ENVIRONMENT);
    cmdTypeBuilder.AddMemberWithValue("LC_MAIN", LC_MAIN);//                  (0x28 | LC_REQ_DYLD)
    cmdTypeBuilder.AddMemberWithValue("LC_DATA_IN_CODE", LC_DATA_IN_CODE);
    cmdTypeBuilder.AddMemberWithValue("LC_SOURCE_VERSION", LC_SOURCE_VERSION);
    cmdTypeBuilder.AddMemberWithValue("LC_DYLIB_CODE_SIGN_DRS", LC_DYLIB_CODE_SIGN_DRS);
    cmdTypeBuilder.AddMemberWithValue("LC_ENCRYPTION_INFO_64", _LC_ENCRYPTION_INFO_64);
    cmdTypeBuilder.AddMemberWithValue("LC_LINKER_OPTION", _LC_LINKER_OPTION);
    cmdTypeBuilder.AddMemberWithValue("LC_LINKER_OPTIMIZATION_HINT", _LC_LINKER_OPTIMIZATION_HINT);
    cmdTypeBuilder.AddMemberWithValue("LC_VERSION_MIN_TVOS", _LC_VERSION_MIN_TVOS);
    cmdTypeBuilder.AddMemberWithValue("LC_VERSION_MIN_WATCHOS", LC_VERSION_MIN_WATCHOS);
    cmdTypeBuilder.AddMemberWithValue("LC_NOTE", LC_NOTE);
    cmdTypeBuilder.AddMemberWithValue("LC_BUILD_VERSION", LC_BUILD_VERSION);
    cmdTypeBuilder.AddMemberWithValue("LC_DYLD_EXPORTS_TRIE", LC_DYLD_EXPORTS_TRIE);
    cmdTypeBuilder.AddMemberWithValue("LC_DYLD_CHAINED_FIXUPS", LC_DYLD_CHAINED_FIXUPS);
    Ref<Enumeration> cmdTypeEnum = cmdTypeBuilder.Finalize();

    Ref<Type> cmdTypeEnumType = Type::EnumerationType(m_arch, cmdTypeEnum, 4, false);
    std::string cmdTypeEnumName = "load_command_type_t";
    std::string cmdTypeEnumId = Type::GenerateAutoTypeId("macho", cmdTypeEnumName);
    m_types.CmdTypeEnum = DefineType(cmdTypeEnumId, cmdTypeEnumName, cmdTypeEnumType);

    StructureBuilder loadCommandBuilder;
    loadCommandBuilder.AddMember(Type::NamedType(this, m_types.CmdTypeEnum), "cmd");
    loadCommandBuilder.AddMember(Type::IntegerType(4, false), "cmdsize");
    Ref<Structure> loadCommandStruct = loadCommandBuilder.Finalize();
    QualifiedName loadCommandName = std::string("load_command");
    std::string loadCommandTypeId = Type::GenerateAutoTypeId("macho", loadCommandName);
    Ref<Type> loadCommandType = Type::StructureType(loadCommandStruct);
    m_types.LoadCommand = DefineType(loadCommandTypeId, loadCommandName, loadCommandType);

    EnumerationBuilder protTypeBuilder;
    protTypeBuilder.AddMemberWithValue("VM_PROT_NONE", MACHO_VM_PROT_NONE);
    protTypeBuilder.AddMemberWithValue("VM_PROT_READ", MACHO_VM_PROT_READ);
    protTypeBuilder.AddMemberWithValue("VM_PROT_WRITE", MACHO_VM_PROT_WRITE);
    protTypeBuilder.AddMemberWithValue("VM_PROT_EXECUTE", MACHO_VM_PROT_EXECUTE);
    // protTypeBuilder.AddMemberWithValue("VM_PROT_DEFAULT", MACHO_VM_PROT_DEFAULT);
    // protTypeBuilder.AddMemberWithValue("VM_PROT_ALL", MACHO_VM_PROT_ALL);
    protTypeBuilder.AddMemberWithValue("VM_PROT_NO_CHANGE", MACHO_VM_PROT_NO_CHANGE);
    protTypeBuilder.AddMemberWithValue("VM_PROT_COPY_OR_WANTS_COPY", MACHO_VM_PROT_COPY);
    //protTypeBuilder.AddMemberWithValue("VM_PROT_WANTS_COPY", MACHO_VM_PROT_WANTS_COPY);
    Ref<Enumeration> protTypeEnum = protTypeBuilder.Finalize();

    Ref<Type> protTypeEnumType = Type::EnumerationType(m_arch, protTypeEnum, 4, false);
    std::string protTypeEnumName = "vm_prot_t";
    std::string protTypeEnumId = Type::GenerateAutoTypeId("macho", protTypeEnumName);
    m_types.ProtTypeEnum = DefineType(protTypeEnumId, protTypeEnumName, protTypeEnumType);

    EnumerationBuilder segFlagsTypeBuilder;
    segFlagsTypeBuilder.AddMemberWithValue("SG_HIGHVM", SG_HIGHVM);
    segFlagsTypeBuilder.AddMemberWithValue("SG_FVMLIB", SG_FVMLIB);
    segFlagsTypeBuilder.AddMemberWithValue("SG_NORELOC", SG_NORELOC);
    segFlagsTypeBuilder.AddMemberWithValue("SG_PROTECTED_VERSION_1", SG_PROTECTED_VERSION_1);
    Ref<Enumeration> segFlagsTypeEnum = segFlagsTypeBuilder.Finalize();

    Ref<Type> segFlagsTypeEnumType = Type::EnumerationType(m_arch, segFlagsTypeEnum, 4,
                                                           false);
    std::string segFlagsTypeEnumName = "sg_flags_t";
    std::string segFlagsTypeEnumId = Type::GenerateAutoTypeId("macho", segFlagsTypeEnumName);
    m_types.SegFlagsTypeEnum = DefineType(segFlagsTypeEnumId, segFlagsTypeEnumName, segFlagsTypeEnumType);

    StructureBuilder loadSegmentCommandBuilder;
    loadSegmentCommandBuilder.AddMember(Type::NamedType(this, m_types.CmdTypeEnum), "cmd");
    loadSegmentCommandBuilder.AddMember(Type::IntegerType(4, false), "cmdsize");
    loadSegmentCommandBuilder.AddMember(Type::ArrayType(Type::IntegerType(1, true), 16), "segname");
    loadSegmentCommandBuilder.AddMember(Type::IntegerType(4, false), "vmaddr");
    loadSegmentCommandBuilder.AddMember(Type::IntegerType(4, false), "vmsize");
    loadSegmentCommandBuilder.AddMember(Type::IntegerType(4, false), "fileoff");
    loadSegmentCommandBuilder.AddMember(Type::IntegerType(4, false), "filesize");
    loadSegmentCommandBuilder.AddMember(Type::NamedType(this, m_types.ProtTypeEnum), "maxprot");
    loadSegmentCommandBuilder.AddMember(Type::NamedType(this, m_types.ProtTypeEnum), "initprot");
    loadSegmentCommandBuilder.AddMember(Type::IntegerType(4, false), "nsects");
    loadSegmentCommandBuilder.AddMember(Type::NamedType(this, m_types.SegFlagsTypeEnum), "flags");
    Ref<Structure> loadSegmentCommandStruct = loadSegmentCommandBuilder.Finalize();
    QualifiedName loadSegmentCommandName = std::string("segment_command");
    std::string loadSegmentCommandTypeId = Type::GenerateAutoTypeId("macho", loadSegmentCommandName);
    Ref<Type> loadSegmentCommandType = Type::StructureType(loadSegmentCommandStruct);
    m_types.LoadSegmentCommandType = DefineType(loadSegmentCommandTypeId, loadSegmentCommandName,
                                                loadSegmentCommandType);

    StructureBuilder loadSegmentCommand64Builder;
    loadSegmentCommand64Builder.AddMember(Type::NamedType(this, m_types.CmdTypeEnum), "cmd");
    loadSegmentCommand64Builder.AddMember(Type::IntegerType(4, false), "cmdsize");
    loadSegmentCommand64Builder.AddMember(Type::ArrayType(Type::IntegerType(1, true), 16), "segname");
    loadSegmentCommand64Builder.AddMember(Type::IntegerType(8, false), "vmaddr");
    loadSegmentCommand64Builder.AddMember(Type::IntegerType(8, false), "vmsize");
    loadSegmentCommand64Builder.AddMember(Type::IntegerType(8, false), "fileoff");
    loadSegmentCommand64Builder.AddMember(Type::IntegerType(8, false), "filesize");
    loadSegmentCommand64Builder.AddMember(Type::NamedType(this, m_types.ProtTypeEnum), "maxprot");
    loadSegmentCommand64Builder.AddMember(Type::NamedType(this, m_types.ProtTypeEnum), "initprot");
    loadSegmentCommand64Builder.AddMember(Type::IntegerType(4, false), "nsects");
    loadSegmentCommand64Builder.AddMember(Type::NamedType(this, m_types.SegFlagsTypeEnum), "flags");
    Ref<Structure> loadSegmentCommand64Struct = loadSegmentCommand64Builder.Finalize();
    QualifiedName loadSegment64CommandName = std::string("segment_command_64");
    std::string loadSegment64CommandTypeId = Type::GenerateAutoTypeId("macho", loadSegment64CommandName);
    Ref<Type> loadSegment64CommandType = Type::StructureType(loadSegmentCommand64Struct);
    m_types.LoadSegmentCommand64Type = DefineType(loadSegment64CommandTypeId, loadSegment64CommandName,
                                                  loadSegment64CommandType);

    StructureBuilder sectionBuilder;
    sectionBuilder.AddMember(Type::ArrayType(Type::IntegerType(1, true), 16), "sectname");
    sectionBuilder.AddMember(Type::ArrayType(Type::IntegerType(1, true), 16), "segname");
    sectionBuilder.AddMember(Type::IntegerType(4, false), "addr");
    sectionBuilder.AddMember(Type::IntegerType(4, false), "size");
    sectionBuilder.AddMember(Type::IntegerType(4, false), "offset");
    sectionBuilder.AddMember(Type::IntegerType(4, false), "align");
    sectionBuilder.AddMember(Type::IntegerType(4, false), "reloff");
    sectionBuilder.AddMember(Type::IntegerType(4, false), "nreloc");
    sectionBuilder.AddMember(Type::IntegerType(4, false), "flags");
    sectionBuilder.AddMember(Type::IntegerType(4, false), "reserved1");
    sectionBuilder.AddMember(Type::IntegerType(4, false), "reserved2");
    Ref<Structure> sectionStruct = sectionBuilder.Finalize();
    QualifiedName sectionName = std::string("section");
    std::string sectionTypeId = Type::GenerateAutoTypeId("macho", sectionName);
    Ref<Type> sectionType = Type::StructureType(sectionStruct);
    m_types.SectionType = DefineType(sectionTypeId, sectionName, sectionType);

    StructureBuilder section64Builder;
    section64Builder.AddMember(Type::ArrayType(Type::IntegerType(1, true), 16), "sectname");
    section64Builder.AddMember(Type::ArrayType(Type::IntegerType(1, true), 16), "segname");
    section64Builder.AddMember(Type::IntegerType(8, false), "addr");
    section64Builder.AddMember(Type::IntegerType(8, false), "size");
    section64Builder.AddMember(Type::IntegerType(4, false), "offset");
    section64Builder.AddMember(Type::IntegerType(4, false), "align");
    section64Builder.AddMember(Type::IntegerType(4, false), "reloff");
    section64Builder.AddMember(Type::IntegerType(4, false), "nreloc");
    section64Builder.AddMember(Type::IntegerType(4, false), "flags");
    section64Builder.AddMember(Type::IntegerType(4, false), "reserved1");
    section64Builder.AddMember(Type::IntegerType(4, false), "reserved2");
    section64Builder.AddMember(Type::IntegerType(4, false), "reserved3");
    Ref<Structure> section64Struct = section64Builder.Finalize();
    QualifiedName section64Name = std::string("section_64");
    std::string section64TypeId = Type::GenerateAutoTypeId("macho", section64Name);
    Ref<Type> section64Type = Type::StructureType(section64Struct);
    m_types.Section64Type = DefineType(section64TypeId, section64Name, section64Type);

    StructureBuilder symtabBuilder;
    symtabBuilder.AddMember(Type::NamedType(this, m_types.CmdTypeEnum), "cmd");
    symtabBuilder.AddMember(Type::IntegerType(4, false), "cmdsize");
    symtabBuilder.AddMember(Type::IntegerType(4, false), "symoff");
    symtabBuilder.AddMember(Type::IntegerType(4, false), "nsyms");
    symtabBuilder.AddMember(Type::IntegerType(4, false), "stroff");
    symtabBuilder.AddMember(Type::IntegerType(4, false), "strsize");
    Ref<Structure> symtabStruct = symtabBuilder.Finalize();
    QualifiedName symtabName = std::string("symtab");
    std::string symtabTypeId = Type::GenerateAutoTypeId("macho", symtabName);
    Ref<Type> symtabType = Type::StructureType(symtabStruct);
    m_types.SymTabType = DefineType(symtabTypeId, symtabName, symtabType);

    StructureBuilder dynsymtabBuilder;
    dynsymtabBuilder.AddMember(Type::NamedType(this, m_types.CmdTypeEnum), "cmd");
    dynsymtabBuilder.AddMember(Type::IntegerType(4, false), "cmdsize");
    dynsymtabBuilder.AddMember(Type::IntegerType(4, false), "ilocalsym");
    dynsymtabBuilder.AddMember(Type::IntegerType(4, false), "nlocalsym");
    dynsymtabBuilder.AddMember(Type::IntegerType(4, false), "iextdefsym");
    dynsymtabBuilder.AddMember(Type::IntegerType(4, false), "nextdefsym");
    dynsymtabBuilder.AddMember(Type::IntegerType(4, false), "iundefsym");
    dynsymtabBuilder.AddMember(Type::IntegerType(4, false), "nundefsym");
    dynsymtabBuilder.AddMember(Type::IntegerType(4, false), "tocoff");
    dynsymtabBuilder.AddMember(Type::IntegerType(4, false), "ntoc");
    dynsymtabBuilder.AddMember(Type::IntegerType(4, false), "modtaboff");
    dynsymtabBuilder.AddMember(Type::IntegerType(4, false), "nmodtab");
    dynsymtabBuilder.AddMember(Type::IntegerType(4, false), "extrefsymoff");
    dynsymtabBuilder.AddMember(Type::IntegerType(4, false), "nextrefsyms");
    dynsymtabBuilder.AddMember(Type::IntegerType(4, false), "indirectsymoff");
    dynsymtabBuilder.AddMember(Type::IntegerType(4, false), "nindirectsyms");
    dynsymtabBuilder.AddMember(Type::IntegerType(4, false), "extreloff");
    dynsymtabBuilder.AddMember(Type::IntegerType(4, false), "nextrel");
    dynsymtabBuilder.AddMember(Type::IntegerType(4, false), "locreloff");
    dynsymtabBuilder.AddMember(Type::IntegerType(4, false), "nlocrel");
    Ref<Structure> dynsymtabStruct = dynsymtabBuilder.Finalize();
    QualifiedName dynsymtabName = std::string("dynsymtab");
    std::string dynsymtabTypeId = Type::GenerateAutoTypeId("macho", dynsymtabName);
    Ref<Type> dynsymtabType = Type::StructureType(dynsymtabStruct);
    m_types.DynSymTabType = DefineType(dynsymtabTypeId, dynsymtabName, dynsymtabType);

    StructureBuilder uuidBuilder;
    uuidBuilder.AddMember(Type::NamedType(this, m_types.CmdTypeEnum), "cmd");
    uuidBuilder.AddMember(Type::IntegerType(4, false), "cmdsize");
    uuidBuilder.AddMember(Type::ArrayType(Type::IntegerType(1, false), 16), "uuid");
    Ref<Structure> uuidStruct = uuidBuilder.Finalize();
    QualifiedName uuidName = std::string("uuid");
    std::string uuidTypeId = Type::GenerateAutoTypeId("macho", uuidName);
    Ref<Type> uuidType = Type::StructureType(uuidStruct);
    m_types.UUIDType = DefineType(uuidTypeId, uuidName, uuidType);

    StructureBuilder linkeditDataBuilder;
    linkeditDataBuilder.AddMember(Type::NamedType(this, m_types.CmdTypeEnum), "cmd");
    linkeditDataBuilder.AddMember(Type::IntegerType(4, false), "cmdsize");
    linkeditDataBuilder.AddMember(Type::IntegerType(4, false), "dataoff");
    linkeditDataBuilder.AddMember(Type::IntegerType(4, false), "datasize");
    Ref<Structure> linkeditDataStruct = linkeditDataBuilder.Finalize();
    QualifiedName linkeditDataName = std::string("linkedit_data");
    std::string linkeditDataTypeId = Type::GenerateAutoTypeId("macho", linkeditDataName);
    Ref<Type> linkeditDataType = Type::StructureType(linkeditDataStruct);
    m_types.LinkEditData = DefineType(linkeditDataTypeId, linkeditDataName, linkeditDataType);

    StructureBuilder encryptionInfoBuilder;
    encryptionInfoBuilder.AddMember(Type::NamedType(this, m_types.CmdTypeEnum), "cmd");
    encryptionInfoBuilder.AddMember(Type::IntegerType(4, false), "cmdsize");
    encryptionInfoBuilder.AddMember(Type::IntegerType(4, false), "cryptoff");
    encryptionInfoBuilder.AddMember(Type::IntegerType(4, false), "cryptsize");
    encryptionInfoBuilder.AddMember(Type::IntegerType(4, false), "cryptid");
    Ref<Structure> encryptionInfoStruct = encryptionInfoBuilder.Finalize();
    QualifiedName encryptionInfoName = std::string("encryption_info");
    std::string encryptionInfoTypeId = Type::GenerateAutoTypeId("macho", encryptionInfoName);
    Ref<Type> encryptionInfoType = Type::StructureType(encryptionInfoStruct);
    m_types.EncryptionInfoType = DefineType(encryptionInfoTypeId, encryptionInfoName, encryptionInfoType);

    StructureBuilder versionMinBuilder;
    versionMinBuilder.AddMember(Type::NamedType(this, m_types.CmdTypeEnum), "cmd");
    versionMinBuilder.AddMember(Type::IntegerType(4, false), "cmdsize");
    versionMinBuilder.AddMember(Type::IntegerType(4, false), "version");
    versionMinBuilder.AddMember(Type::IntegerType(4, false), "sdk");
    Ref<Structure> versionMinStruct = versionMinBuilder.Finalize();
    QualifiedName versionMinName = std::string("version_min");
    std::string versionMinTypeId = Type::GenerateAutoTypeId("macho", versionMinName);
    Ref<Type> versionMinType = Type::StructureType(versionMinStruct);
    m_types.VersionMinType = DefineType(versionMinTypeId, versionMinName, versionMinType);

    StructureBuilder dyldInfoBuilder;
    dyldInfoBuilder.AddMember(Type::NamedType(this, m_types.CmdTypeEnum), "cmd");
    dyldInfoBuilder.AddMember(Type::IntegerType(4, false), "cmdsize");
    dyldInfoBuilder.AddMember(Type::IntegerType(4, false), "rebase_off");
    dyldInfoBuilder.AddMember(Type::IntegerType(4, false), "rebase_size");
    dyldInfoBuilder.AddMember(Type::IntegerType(4, false), "bind_off");
    dyldInfoBuilder.AddMember(Type::IntegerType(4, false), "bind_size");
    dyldInfoBuilder.AddMember(Type::IntegerType(4, false), "weak_bind_off");
    dyldInfoBuilder.AddMember(Type::IntegerType(4, false), "weak_bind_size");
    dyldInfoBuilder.AddMember(Type::IntegerType(4, false), "lazy_bind_off");
    dyldInfoBuilder.AddMember(Type::IntegerType(4, false), "lazy_bind_size");
    dyldInfoBuilder.AddMember(Type::IntegerType(4, false), "export_off");
    dyldInfoBuilder.AddMember(Type::IntegerType(4, false), "export_size");
    Ref<Structure> dyldInfoStruct = dyldInfoBuilder.Finalize();
    QualifiedName dyldInfoName = std::string("dyld_info");
    std::string dyldInfoTypeId = Type::GenerateAutoTypeId("macho", dyldInfoName);
    Ref<Type> dyldInfoType = Type::StructureType(dyldInfoStruct);
    m_types.DyldInfoType = DefineType(dyldInfoTypeId, dyldInfoName, dyldInfoType);

    StructureBuilder dylibBuilder;
    dylibBuilder.AddMember(Type::IntegerType(4, false), "name");
    dylibBuilder.AddMember(Type::IntegerType(4, false), "timestamp");
    dylibBuilder.AddMember(Type::IntegerType(4, false), "current_version");
    dylibBuilder.AddMember(Type::IntegerType(4, false), "compatibility_version");
    Ref<Structure> dylibStruct = dylibBuilder.Finalize();
    QualifiedName dylibName = std::string("dylib");
    std::string dylibTypeId = Type::GenerateAutoTypeId("macho", dylibName);
    Ref<Type> dylibType = Type::StructureType(dylibStruct);
    m_types.DylibType = DefineType(dylibTypeId, dylibName, dylibType);

    StructureBuilder dylibCommandBuilder;
    dylibCommandBuilder.AddMember(Type::NamedType(this, m_types.CmdTypeEnum), "cmd");
    dylibCommandBuilder.AddMember(Type::IntegerType(4, false), "cmdsize");
    dylibCommandBuilder.AddMember(Type::NamedType(this, m_types.DylibType), "dylib");
    Ref<Structure> dylibCommandStruct = dylibCommandBuilder.Finalize();
    QualifiedName dylibCommandName = std::string("dylib_command");
    std::string dylibCommandTypeId = Type::GenerateAutoTypeId("macho", dylibCommandName);
    Ref<Type> dylibCommandType = Type::StructureType(dylibCommandStruct);
    m_types.DylibCommandType = DefineType(dylibCommandTypeId, dylibCommandName, dylibCommandType);

    m_typesLoaded = true;
}


DSCViewType::DSCViewType() : BinaryViewType("DSC", "DSC") {
}

Ref<BinaryView> DSCViewType::Create(BinaryView *data) {
    try {
        auto fileView = new DSCFileView("DSC", data, true);
        g_dscViewInstance = new DSCView("DSC", fileView, false);
        return g_dscViewInstance;
    } catch (std::exception &e) {
        LogError("%s<BinaryViewType> failed to create view! '%s'", "DSC", e.what());
        return nullptr;
    }
}


Ref<BinaryView> DSCViewType::Parse(BinaryView *data) {
    try {
        auto fileView = new DSCFileView("DSC", data, true);
        g_dscViewInstance = new DSCView("DSC", fileView, true);
        return g_dscViewInstance;
    } catch (std::exception &e) {
        LogError("%s<BinaryViewType> failed to create view! '%s'", "DSC", e.what());
        return nullptr;
    }
}


bool DSCViewType::IsTypeValidForData(BinaryView *data) {
    if (!data)
        return false;

    DataBuffer sig = data->ReadBuffer(data->GetStart(), 4);
    if (sig.GetLength() != 4)
        return false;

    const char *magic = (char *) sig.GetData();
    if (strncmp(magic, "dyld", 4) == 0)
        return true;

    return false;
}


DSCRawView::DSCRawView(BinaryView* data, bool parseOnly) : BinaryView("DSCRaw", data->GetFile(),
                                                                      data)
{
    std::map<QualifiedName, Ref<Type>> types, variables, functions;
    std::string errors;

    SetDefaultPlatform(Platform::GetByName("mac-aarch64"));

    GetDefaultPlatform()->ParseTypesFromSource(dyldRawTypes, "a", types, variables, functions, errors);

    for (const auto& [name, type] : types) {
        DefineUserType(name, type);
    }

    DefineUserDataVariable(0, GetTypeByName({"dyld_cache_header"}));
}

bool DSCRawView::Init()
{
    AddAutoSegment(0, GetParentView()->GetLength(), 0, GetParentView()->GetLength(), SegmentReadable | SegmentWritable);

    return false;
}

DSCRawViewType::DSCRawViewType() : BinaryViewType("DSCRaw", "DSCRaw") {
}

Ref<BinaryView> DSCRawViewType::Create(BinaryView* data)
{
    try {
        return new DSCRawView(data, true);
    } catch (std::exception &e) {
        LogError("%s<BinaryViewType> failed to create view! '%s'", "DSC", e.what());
        return nullptr;
    }
}

Ref<BinaryView> DSCRawViewType::Parse(BinaryView* data)
{
    try {
        return new DSCRawView(data, true);
    } catch (std::exception &e) {
        LogError("%s<BinaryViewType> failed to create view! '%s'", "DSC", e.what());
        return nullptr;
    }
}

bool DSCRawViewType::IsTypeValidForData(BinaryView *data)
{
    if (!data)
        return false;

    DataBuffer sig = data->ReadBuffer(data->GetStart(), 4);
    if (sig.GetLength() != 4)
        return false;

    const char *magic = (char *) sig.GetData();
    if (strncmp(magic, "dyld", 4) == 0)
        return true;

    return false;
}

void InitDSCViewType() {
    static DSCRawViewType rawType;
    BinaryViewType::Register(&rawType);
    static DSCViewType type;
    BinaryViewType::Register(&type);
    g_dscViewType = &type;
    g_dscRawViewType = &rawType;
}


bool BNDSCViewLoadImageWithInstallName(BNBinaryView* view, char* name)
{
    std::string imageName = std::string(name);
    BNFreeString(name);
    uint64_t sessionID = BNFileMetadataGetSessionId(BNGetFileForView(view));
    if (DSCView* dscView = g_dscViews[sessionID])
    {
        dscView->LoadImageViaInstallName(imageName);
        return true;
    }
    return false;
}

char** BNDSCViewGetInstallNames(BNBinaryView *view, size_t* count)
{
    uint64_t sessionID = BNFileMetadataGetSessionId(BNGetFileForView(view));
    if (DSCView* dscView = g_dscViews[sessionID])
    {
        auto value = dscView->m_cache->m_installNames;
        *count = value.size();

        std::vector<const char*> cstrings;
        for (size_t i = 0; i < value.size(); i ++)
        {
            cstrings.push_back(value[i].c_str());
        }
        return BNAllocStringList(cstrings.data(), cstrings.size());
    }
    *count = 0;
    return nullptr;
}

extern "C" {

BN_DECLARE_CORE_ABI_VERSION

BINARYNINJAPLUGIN bool CorePluginInit() {

    InitDSCViewType();

    return true;
}

}