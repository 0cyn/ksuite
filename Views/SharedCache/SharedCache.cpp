//
// Created by kat on 5/19/23.
//

#include "SharedCache.h"
#include <binaryninjaapi.h>
#include <ksuiteapi.h>
#include "highlevelilinstruction.h"
#include "../MachO/machoview.h"
#include <filesystem>
#include <utility>
#include <csignal>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <memory>

using namespace BinaryNinja;


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
        BNLogInfo("Couldn't read file at %s", path.c_str());
        throw MissingFileException();
    }
    m_mmap.Map();
}

MMappedFileAccessor::~MMappedFileAccessor() {
    BNLogInfo("Unmapping %s", m_path.c_str());
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
    for (auto &[key, value]: m_map) {
        value.file.reset();
    }
}


void VM::MapPages(size_t vm_address, size_t fileoff, size_t size, std::shared_ptr<MMappedFileAccessor> file) {
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

VMReader::VMReader(std::shared_ptr<VM> vm, size_t addressSize) : m_vm(vm), m_cursor(0), m_addressSize(addressSize) {
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


DSCRawView::DSCRawView(const std::string &typeName, BinaryView *data, bool parseOnly)
    : BinaryView(typeName,
            data->GetFile(),
            data)
{
    m_filename = data->GetFile()->GetFilename();
}

bool DSCRawView::Init()
{
    auto reader = new BinaryReader(GetParentView());
    reader->Seek(16);
    auto size = reader->Read32();
    AddAutoSegment(0, size, 0, size, SegmentReadable);

    return true;
}

DSCRawViewType::DSCRawViewType() : BinaryViewType("DSCRaw", "DSCRaw") {
}

BinaryNinja::Ref<BinaryNinja::BinaryView> DSCRawViewType::Create(BinaryView* data)
{
    return new DSCRawView("DSCRaw", data, false);
}

BinaryNinja::Ref<BinaryNinja::BinaryView> DSCRawViewType::Parse(BinaryView* data)
{
    return new DSCRawView("DSCRaw", data, true);
}

bool DSCRawViewType::IsTypeValidForData(BinaryNinja::BinaryView *data)
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

bool SharedCache::SetupVMMap(bool mapPages)
{
    auto path = m_rawView->GetFile()->GetFilename();
    try {
        m_baseFile = std::shared_ptr<MMappedFileAccessor>(new MMappedFileAccessor(path));
    }
    catch (MissingFileException& exc)
    {
        return false;
    }
    if (mapPages)
    {
        auto format = GetCacheFormat();

        dyld_cache_header header{};
        size_t header_size = m_baseFile->ReadUInt32(16);
        m_baseFile->Read(&header, 0, std::min(header_size, sizeof(dyld_cache_header)));

        switch (format) {
            case RegularCacheFormat: {
                m_vm = std::shared_ptr<VM>(new VM(0x4000)); // TODO: way to absolutely determine page size

                dyld_cache_mapping_info mapping{};

                for (size_t i = 0; i < header.mappingCount; i++) {
                    m_baseFile->Read(&mapping, header.mappingOffset + (i * sizeof(mapping)),
                               sizeof(mapping));
                    m_vm->MapPages(mapping.address, mapping.fileOffset, mapping.size, m_baseFile);
                }
                m_pagesMapped = true;
            }
            case LargeCacheFormat: {
                m_vm = std::shared_ptr<VM>(new VM(0x4000));

                // First, map our main cache.

                dyld_cache_mapping_info mapping{}; // We're going to reuse this for all of the mappings. We only need it briefly.

                for (size_t i = 0; i < header.mappingCount; i++) {
                    m_baseFile->Read(&mapping, header.mappingOffset + (i * sizeof(mapping)),
                               sizeof(mapping));
                    m_vm->MapPages(mapping.address, mapping.fileOffset, mapping.size, m_baseFile);
                }

                auto mainFileName = m_baseFile->Path();
                auto subCacheCount = header.subCacheArrayCount;
                dyld_subcache_entry2 entry{};

                for (size_t i = 0; i < subCacheCount; i++) {

                    m_baseFile->Read(&entry, header.subCacheArrayOffset + (i * sizeof(dyld_subcache_entry2)),
                               sizeof(dyld_subcache_entry2));

                    std::string subCachePath;
                    if (std::string(entry.fileExtension).find('.') != std::string::npos)
                        subCachePath = mainFileName + entry.fileExtension;
                    else
                        subCachePath = mainFileName + "." + entry.fileExtension;
                    auto subCacheFile = std::shared_ptr<MMappedFileAccessor>(new MMappedFileAccessor(subCachePath));

                    auto header_size = subCacheFile->ReadUInt32(16);
                    dyld_cache_header header{};
                    subCacheFile->Read(&header, 0, header_size);

                    dyld_cache_mapping_info subCacheMapping{};

                    for (size_t j = 0; j < header.mappingCount; j++) {

                        subCacheFile->Read(&subCacheMapping, header.mappingOffset + (j * sizeof(subCacheMapping)),
                                           sizeof(subCacheMapping));
                        m_vm->MapPages(subCacheMapping.address, subCacheMapping.fileOffset, subCacheMapping.size,
                                              subCacheFile);
                    }
                }
                m_pagesMapped = true;
            }
            case SplitCacheFormat: {
                m_vm = std::shared_ptr<VM>(new VM(0x4000));

                // First, map our main cache.

                dyld_cache_mapping_info mapping{}; // We're going to reuse this for all of the mappings. We only need it briefly.

                for (size_t i = 0; i < header.mappingCount; i++) {
                    m_baseFile->Read(&mapping, header.mappingOffset + (i * sizeof(mapping)),
                               sizeof(mapping));
                    m_vm->MapPages(mapping.address, mapping.fileOffset, mapping.size, m_baseFile);
                }

                auto mainFileName = m_baseFile->Path();
                auto subCacheCount = header.subCacheArrayCount;

                for (size_t i = 1; i <= subCacheCount; i++) {
                    auto subCachePath = mainFileName + "." + std::to_string(i);
                    auto subCacheFile = std::shared_ptr<MMappedFileAccessor>(new MMappedFileAccessor(subCachePath));
                    auto header_size = subCacheFile->ReadUInt32(16);
                    dyld_cache_header header{};
                    subCacheFile->Read(&header, 0, header_size);

                    dyld_cache_mapping_info subCacheMapping{};

                    for (size_t j = 0; j < header.mappingCount; j++) {
                        subCacheFile->Read(&subCacheMapping, header.mappingOffset + (j * sizeof(subCacheMapping)),
                                           sizeof(subCacheMapping));
                        m_vm->MapPages(subCacheMapping.address, subCacheMapping.fileOffset, subCacheMapping.size,
                                              subCacheFile);
                    }
                }

                // Load .symbols subcache

                auto subCachePath = mainFileName + ".symbols";
                auto subCacheFile = std::shared_ptr<MMappedFileAccessor>(new MMappedFileAccessor(subCachePath));

                auto subcache_header_size = subCacheFile->ReadUInt32(16);
                dyld_cache_header subcacheHeader{};
                subCacheFile->Read(&subcacheHeader, 0, subcache_header_size);

                dyld_cache_mapping_info subCacheMapping{};

                for (size_t j = 0; j < subcacheHeader.mappingCount; j++) {
                    subCacheFile->Read(&subCacheMapping, subcacheHeader.mappingOffset + (j * sizeof(subCacheMapping)),
                                       sizeof(subCacheMapping));
                    m_vm->MapPages(subCacheMapping.address, subCacheMapping.fileOffset, subCacheMapping.size,
                                          subCacheFile);
                }
                m_pagesMapped = true;
            }
            case iOS16CacheFormat: {
                m_vm = std::shared_ptr<VM>(new VM(0x4000));

                // First, map our main cache.

                dyld_cache_mapping_info mapping{};

                for (size_t i = 0; i < header.mappingCount; i++) {
                    m_baseFile->Read(&mapping, header.mappingOffset + (i * sizeof(mapping)),
                               sizeof(mapping));
                    m_vm->MapPages(mapping.address, mapping.fileOffset, mapping.size, m_baseFile);
                }

                auto mainFileName = m_baseFile->Path();
                auto subCacheCount = header.subCacheArrayCount;

                dyld_subcache_entry2 entry{};

                for (size_t i = 0; i < subCacheCount; i++) {

                    m_baseFile->Read(&entry, header.subCacheArrayOffset + (i * sizeof(dyld_subcache_entry2)),
                               sizeof(dyld_subcache_entry2));

                    std::string subCachePath;
                    if (std::string(entry.fileExtension).find('.') != std::string::npos)
                        subCachePath = mainFileName + entry.fileExtension;
                    else
                        subCachePath = mainFileName + "." + entry.fileExtension;

                    auto subCacheFile = std::shared_ptr<MMappedFileAccessor>(new MMappedFileAccessor(subCachePath));

                    auto header_size = subCacheFile->ReadUInt32(16);
                    dyld_cache_header header{};
                    subCacheFile->Read(&header, 0, header_size);

                    dyld_cache_mapping_info subCacheMapping{};

                    for (size_t j = 0; j < header.mappingCount; j++) {

                        subCacheFile->Read(&subCacheMapping, header.mappingOffset + (j * sizeof(subCacheMapping)),
                                           sizeof(subCacheMapping));
                        m_vm->MapPages(subCacheMapping.address, subCacheMapping.fileOffset, subCacheMapping.size,
                                              subCacheFile);
                    }
                }

                // Load .symbols subcache
                try {
                    auto subCachePath = mainFileName + ".symbols";
                    auto subCacheFile = std::shared_ptr<MMappedFileAccessor>(new MMappedFileAccessor(subCachePath));
                    auto header_size = subCacheFile->ReadUInt32(16);
                    dyld_cache_header header{};
                    subCacheFile->Read(&header, 0, header_size);

                    dyld_cache_mapping_info subCacheMapping{};

                    for (size_t j = 0; j < header.mappingCount; j++) {
                        subCacheFile->Read(&subCacheMapping, header.mappingOffset + (j * sizeof(subCacheMapping)),
                                           sizeof(subCacheMapping));
                        m_vm->MapPages(subCacheMapping.address, subCacheMapping.fileOffset, subCacheMapping.size,
                                              subCacheFile);
                    }
                } catch (...) {

                }
                m_pagesMapped = true;
            }
        }
    }
    return true;
}
bool SharedCache::TeardownVMMap()
{
    m_baseFile.reset();
    if (m_pagesMapped)
        m_vm.reset();
    return true;
}

SharedCache::SharedCacheFormat SharedCache::GetCacheFormat()
{
    // The second argument to this MUST be false.
    // Mapping the pages invokes this function.
    if (!m_baseFile)
        auto mapLock = ScopedVMMapSession(this, false);

    dyld_cache_header header{};
    size_t header_size = m_baseFile->ReadUInt32(16);
    m_baseFile->Read(&header, 0, std::min(header_size, sizeof(dyld_cache_header)));

    if (header.imagesCountOld != 0)
        return RegularCacheFormat;

    size_t subCacheOff = offsetof(struct dyld_cache_header, subCacheArrayOffset);
    size_t headerEnd = header.mappingOffset;
    if (headerEnd > subCacheOff) {
        if (header.cacheType != 2)
        {
            if (std::filesystem::exists(m_baseFile->Path() + ".01"))
                return LargeCacheFormat;
            return SplitCacheFormat;
        }
        else
            return iOS16CacheFormat;
    }

    return RegularCacheFormat;
}

std::string SharedCache::Serialize()
{
    std::stringstream ss;

    cereal::JSONOutputArchive oar(ss);
    serialize(oar);
    return ss.str();
}

void SharedCache::DeserializeFromRawView()
{
    if (m_rawView->QueryMetadata(SharedCacheMetadataTag))
    {
        std::string data = m_rawView->GetStringMetadata(SharedCacheMetadataTag);
        std::stringstream ss;
        ss.str(data);
        cereal::JSONInputArchive iar(ss);

        iar(m_rawViewCursor);
        iar(m_viewState);
        iar(m_loadedImages);
    }
    else
    {
        m_viewState = Unloaded;
        m_loadedImages.clear();
        m_rawViewCursor = m_rawView->GetEnd();
    }
}

SharedCache::SharedCache(BinaryNinja::Ref<BinaryNinja::BinaryView> rawView)
    : m_rawView(rawView)
{
    DeserializeFromRawView();
}

SharedCache* SharedCache::GetFromRawView(BinaryNinja::Ref<BinaryNinja::BinaryView> rawView)
{
    return new SharedCache(std::move(rawView));
}

bool SharedCache::LoadImageWithInstallName(std::string installName)
{
    auto mapLock = ScopedVMMapSession(this);
    auto format = GetCacheFormat();
    LoadedImage image;
    image.headerBase = 0;

    dyld_cache_header header{};
    size_t header_size = m_baseFile->ReadUInt32(16);
    m_baseFile->Read(&header, 0, std::min(header_size, sizeof(dyld_cache_header)));

    switch (format) {
        case RegularCacheFormat: {
            dyld_cache_image_info img{};

            for (size_t i = 0; i < header.imagesCountOld; i++) {
                m_baseFile->Read(&img, header.imagesOffsetOld + (i * sizeof(img)), sizeof(img));
                auto iname = m_baseFile->ReadNullTermString(img.pathFileOffset);
                if (iname == installName)
                {
                    image.headerBase = img.address;
                    image.name = iname;
                    break;
                }
            }
            break;
        }
        case iOS16CacheFormat:
        case SplitCacheFormat:
        case LargeCacheFormat: {
            dyld_cache_image_info img{};

            for (size_t i = 0; i < header.imagesCount; i++) {
                m_baseFile->Read(&img, header.imagesOffset + (i * sizeof(img)), sizeof(img));
                auto iname = m_baseFile->ReadNullTermString(img.pathFileOffset);
                if (iname == installName)
                {
                    image.headerBase = img.address;
                    image.name = iname;
                    break;
                }
            }

            break;
        }
    }

    if (!image.headerBase)
        return false;

    auto reader = VMReader(m_vm);
    size_t headerStart = reader.Offset();
    auto magic = reader.ReadUInt32(headerStart);
    bool is64 = (magic == MH_MAGIC_64 || magic == MH_CIGAM_64);

    if (is64) {
        mach_header_64 header{};
        reader.Read(&header, headerStart, sizeof(mach_header_64));
        size_t off = headerStart + sizeof(mach_header_64);
        for (size_t i = 0; i < header.ncmds; i++) {
            size_t lc = reader.ReadUInt32(off);
            size_t bump = reader.ReadUInt32();
            if (lc == LC_SEGMENT_64) {
                segment_command_64 cmd{};
                reader.Read(&cmd, off, sizeof(segment_command_64));
                m_rawView->WriteBuffer(m_rawViewCursor, *reader.ReadBuffer(cmd.vmaddr, cmd.vmsize));
                image.loadedSegments.push_back({m_rawViewCursor, {cmd.vmaddr, cmd.vmaddr + cmd.vmsize}});
                m_rawViewCursor = m_rawView->GetEnd();
            }
            off += bump;
        }
    } else {
        mach_header header{};
        reader.Read(&header, headerStart, sizeof(mach_header));
        size_t off = headerStart + sizeof(mach_header);
        for (size_t i = 0; i < header.ncmds; i++) {
            size_t lc = reader.ReadUInt32(off);
            size_t bump = reader.ReadUInt32();
            if (lc == LC_SEGMENT) {
                segment_command cmd{};
                reader.Read(&cmd, off, sizeof(segment_command));
                m_rawView->WriteBuffer(m_rawViewCursor, *reader.ReadBuffer(cmd.vmaddr, cmd.vmsize));
                image.loadedSegments.push_back({m_rawViewCursor, {cmd.vmaddr, cmd.vmaddr + cmd.vmsize}});
                m_rawViewCursor = m_rawView->GetEnd();
            }
            off += bump;
        }
    }

    m_loadedImages[image.name] = image;
    SaveToRawView();

    return true;
}

std::vector<std::string> SharedCache::GetAvailableImages()
{
    std::vector<std::string> installNames;
    auto mapLock = ScopedVMMapSession(this);
    auto format = GetCacheFormat();

    dyld_cache_header header{};
    size_t header_size = m_baseFile->ReadUInt32(16);
    m_baseFile->Read(&header, 0, std::min(header_size, sizeof(dyld_cache_header)));

    switch (format) {
        case RegularCacheFormat: {
            dyld_cache_image_info image{};

            for (size_t i = 0; i < header.imagesCountOld; i++) {
                m_baseFile->Read(&image, header.imagesOffsetOld + (i * sizeof(image)), sizeof(image));
                auto iname = m_baseFile->ReadNullTermString(image.pathFileOffset);

                installNames.push_back(iname);
            }
            break;
        }
        case iOS16CacheFormat:
        case SplitCacheFormat:
        case LargeCacheFormat: {
            dyld_cache_image_info image{};

            for (size_t i = 0; i < header.imagesCount; i++) {
                m_baseFile->Read(&image, header.imagesOffset + (i * sizeof(image)), sizeof(image));
                auto iname = m_baseFile->ReadNullTermString(image.pathFileOffset);

                installNames.push_back(iname);
            }

            break;
        }
    }

    return installNames;
}


extern "C" {
char **BNDSCViewGetInstallNames(BNBinaryView *view, size_t *count)
{
    auto rawView = new BinaryView(view);

    if (auto cache = SharedCache::GetFromRawView(rawView))
    {
        auto value = cache->GetAvailableImages();
        *count = value.size();

        std::vector<const char *> cstrings;
        for (size_t i = 0; i < value.size(); i++)
        {
            cstrings.push_back(value[i].c_str());
        }
        return BNAllocStringList(cstrings.data(), cstrings.size());
    }
    *count = 0;
    return nullptr;
}
}

DSCViewType *g_dscViewType;
DSCRawViewType *g_dscRawViewType;

void InitDSCViewType() {
    static DSCRawViewType rawType;
    BinaryViewType::Register(&rawType);
    //static DSCViewType type;
    //BinaryViewType::Register(&type);
    //g_dscViewType = &type;
    g_dscRawViewType = &rawType;

    PluginCommand::Register("List Images", "List Images", [](BinaryView* view){
        auto cache = KAPI::SharedCache(view);
        for (const auto& s : cache.GetAvailableImages())
        {
            BNLogInfo("a %s", s.c_str());
        }
    });
}

