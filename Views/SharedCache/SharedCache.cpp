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
        // BNLogInfo("Couldn't read file at %s", path.c_str());
        throw MissingFileException();
    }
    m_mmap.Map();
}

MMappedFileAccessor::~MMappedFileAccessor() {
    // BNLogInfo("Unmapping %s", m_path.c_str());
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
    GetFile()->SetFilename(data->GetFile()->GetOriginalFilename());
    auto reader = new BinaryReader(GetParentView());
    reader->Seek(16);
    auto size = reader->Read32();
    AddAutoSegment(0, size, 0, size, SegmentReadable);
    GetParentView()->WriteBuffer(0, GetParentView()->ReadBuffer(0, size));
}

bool DSCRawView::Init()
{
    //  AddAutoSegment(0, size, 0, size, SegmentReadable);

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
    return false;
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



DSCView::DSCView(const std::string &typeName, BinaryView *data, bool parseOnly)
        : BinaryView(typeName,
                     data->GetFile(),
                     data)
{
    // m_filename = data->GetFile()->GetFilename();
}

bool DSCView::Init()
{
    SetDefaultArchitecture(Architecture::GetByName("aarch64"));
    SetDefaultPlatform(Platform::GetByName("mac-aarch64"));


    // Add Mach-O file header type info
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

    Ref<Type> cpuTypeEnumType = Type::EnumerationType(nullptr, cpuTypeEnum, 4, false);
    std::string cpuTypeEnumName = "cpu_type_t";
    std::string cpuTypeEnumId = Type::GenerateAutoTypeId("macho", cpuTypeEnumName);
    DefineType(cpuTypeEnumId, cpuTypeEnumName, cpuTypeEnumType);

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
    fileTypeBuilder.AddMemberWithValue("MH_FILESET", MH_FILESET);
    Ref<Enumeration> fileTypeEnum = fileTypeBuilder.Finalize();

    Ref<Type> fileTypeEnumType = Type::EnumerationType(nullptr, fileTypeEnum, 4, false);
    std::string fileTypeEnumName = "file_type_t";
    std::string fileTypeEnumId = Type::GenerateAutoTypeId("macho", fileTypeEnumName);
    DefineType(fileTypeEnumId, fileTypeEnumName, fileTypeEnumType);

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
    flagsTypeBuilder.AddMemberWithValue("MH_NLIST_OUTOFSYNC_WITH_DYLDINFO", _MH_NLIST_OUTOFSYNC_WITH_DYLDINFO);
    flagsTypeBuilder.AddMemberWithValue("MH_SIM_SUPPORT", _MH_SIM_SUPPORT);
    flagsTypeBuilder.AddMemberWithValue("MH_DYLIB_IN_CACHE", _MH_DYLIB_IN_CACHE);
    Ref<Enumeration> flagsTypeEnum = flagsTypeBuilder.Finalize();

    Ref<Type> flagsTypeEnumType = Type::EnumerationType(nullptr, flagsTypeEnum, 4, false);
    std::string flagsTypeEnumName = "flags_type_t";
    std::string flagsTypeEnumId = Type::GenerateAutoTypeId("macho", flagsTypeEnumName);
    DefineType(flagsTypeEnumId, flagsTypeEnumName, flagsTypeEnumType);

    StructureBuilder machoHeaderBuilder;
    machoHeaderBuilder.AddMember(Type::IntegerType(4, false), "magic");
    machoHeaderBuilder.AddMember(Type::NamedType(this, QualifiedName("cpu_type_t")), "cputype");
    machoHeaderBuilder.AddMember(Type::IntegerType(4, false), "cpusubtype");
    machoHeaderBuilder.AddMember(Type::NamedType(this, QualifiedName("file_type_t")), "filetype");
    machoHeaderBuilder.AddMember(Type::IntegerType(4, false), "ncmds");
    machoHeaderBuilder.AddMember(Type::IntegerType(4, false), "sizeofcmds");
    machoHeaderBuilder.AddMember(Type::NamedType(this, QualifiedName("flags_type_t")), "flags");
    if (GetAddressSize() == 8)
        machoHeaderBuilder.AddMember(Type::IntegerType(4, false), "reserved");
    Ref<Structure> machoHeaderStruct = machoHeaderBuilder.Finalize();
    QualifiedName headerName = (GetAddressSize() == 8) ? std::string("mach_header_64") : std::string("mach_header");

    std::string headerTypeId = Type::GenerateAutoTypeId("macho", headerName);
    Ref<Type> machoHeaderType = Type::StructureType(machoHeaderStruct);
    DefineType(headerTypeId, headerName, machoHeaderType);

    EnumerationBuilder cmdTypeBuilder;
    cmdTypeBuilder.AddMemberWithValue("LC_REQ_DYLD", LC_REQ_DYLD);
    cmdTypeBuilder.AddMemberWithValue("LC_SEGMENT", LC_SEGMENT);
    cmdTypeBuilder.AddMemberWithValue("LC_SYMTAB", LC_SYMTAB);
    cmdTypeBuilder.AddMemberWithValue("LC_SYMSEG",LC_SYMSEG);
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
    cmdTypeBuilder.AddMemberWithValue("LC_FILESET_ENTRY", LC_FILESET_ENTRY);
    Ref<Enumeration> cmdTypeEnum = cmdTypeBuilder.Finalize();

    Ref<Type> cmdTypeEnumType = Type::EnumerationType(nullptr, cmdTypeEnum, 4, false);
    std::string cmdTypeEnumName = "load_command_type_t";
    std::string cmdTypeEnumId = Type::GenerateAutoTypeId("macho", cmdTypeEnumName);
    DefineType(cmdTypeEnumId, cmdTypeEnumName, cmdTypeEnumType);

    StructureBuilder loadCommandBuilder;
    loadCommandBuilder.AddMember(Type::NamedType(this, QualifiedName("load_command_type_t")), "cmd");
    loadCommandBuilder.AddMember(Type::IntegerType(4, false), "cmdsize");
    Ref<Structure> loadCommandStruct = loadCommandBuilder.Finalize();
    QualifiedName loadCommandName = std::string("load_command");
    std::string loadCommandTypeId = Type::GenerateAutoTypeId("macho", loadCommandName);
    Ref<Type> loadCommandType = Type::StructureType(loadCommandStruct);
    DefineType(loadCommandTypeId, loadCommandName, loadCommandType);

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

    Ref<Type> protTypeEnumType = Type::EnumerationType(nullptr, protTypeEnum, 4, false);
    std::string protTypeEnumName = "vm_prot_t";
    std::string protTypeEnumId = Type::GenerateAutoTypeId("macho", protTypeEnumName);
    DefineType(protTypeEnumId, protTypeEnumName, protTypeEnumType);

    EnumerationBuilder segFlagsTypeBuilder;
    segFlagsTypeBuilder.AddMemberWithValue("SG_HIGHVM", SG_HIGHVM);
    segFlagsTypeBuilder.AddMemberWithValue("SG_FVMLIB", SG_FVMLIB);
    segFlagsTypeBuilder.AddMemberWithValue("SG_NORELOC", SG_NORELOC);
    segFlagsTypeBuilder.AddMemberWithValue("SG_PROTECTED_VERSION_1", SG_PROTECTED_VERSION_1);
    Ref<Enumeration> segFlagsTypeEnum = segFlagsTypeBuilder.Finalize();

    Ref<Type> segFlagsTypeEnumType = Type::EnumerationType(nullptr, segFlagsTypeEnum, 4, false);
    std::string segFlagsTypeEnumName = "sg_flags_t";
    std::string segFlagsTypeEnumId = Type::GenerateAutoTypeId("macho", segFlagsTypeEnumName);
    DefineType(segFlagsTypeEnumId, segFlagsTypeEnumName, segFlagsTypeEnumType);

    StructureBuilder loadSegmentCommandBuilder;
    loadSegmentCommandBuilder.AddMember(Type::NamedType(this, QualifiedName("load_command_type_t")), "cmd");
    loadSegmentCommandBuilder.AddMember(Type::IntegerType(4, false), "cmdsize");
    loadSegmentCommandBuilder.AddMember(Type::ArrayType(Type::IntegerType(1, true), 16), "segname");
    loadSegmentCommandBuilder.AddMember(Type::IntegerType(4, false), "vmaddr");
    loadSegmentCommandBuilder.AddMember(Type::IntegerType(4, false), "vmsize");
    loadSegmentCommandBuilder.AddMember(Type::IntegerType(4, false), "fileoff");
    loadSegmentCommandBuilder.AddMember(Type::IntegerType(4, false), "filesize");
    loadSegmentCommandBuilder.AddMember(Type::NamedType(this, QualifiedName("vm_prot_t")), "maxprot");
    loadSegmentCommandBuilder.AddMember(Type::NamedType(this, QualifiedName("vm_prot_t")), "initprot");
    loadSegmentCommandBuilder.AddMember(Type::IntegerType(4, false), "nsects");
    loadSegmentCommandBuilder.AddMember(Type::NamedType(this, QualifiedName("sg_flags_t")), "flags");
    Ref<Structure> loadSegmentCommandStruct = loadSegmentCommandBuilder.Finalize();
    QualifiedName loadSegmentCommandName = std::string("segment_command");
    std::string loadSegmentCommandTypeId = Type::GenerateAutoTypeId("macho", loadSegmentCommandName);
    Ref<Type> loadSegmentCommandType = Type::StructureType(loadSegmentCommandStruct);
    DefineType(loadSegmentCommandTypeId, loadSegmentCommandName, loadSegmentCommandType);

    StructureBuilder loadSegmentCommand64Builder;
    loadSegmentCommand64Builder.AddMember(Type::NamedType(this, QualifiedName("load_command_type_t")), "cmd");
    loadSegmentCommand64Builder.AddMember(Type::IntegerType(4, false), "cmdsize");
    loadSegmentCommand64Builder.AddMember(Type::ArrayType(Type::IntegerType(1, true), 16), "segname");
    loadSegmentCommand64Builder.AddMember(Type::IntegerType(8, false), "vmaddr");
    loadSegmentCommand64Builder.AddMember(Type::IntegerType(8, false), "vmsize");
    loadSegmentCommand64Builder.AddMember(Type::IntegerType(8, false), "fileoff");
    loadSegmentCommand64Builder.AddMember(Type::IntegerType(8, false), "filesize");
    loadSegmentCommand64Builder.AddMember(Type::NamedType(this, QualifiedName("vm_prot_t")), "maxprot");
    loadSegmentCommand64Builder.AddMember(Type::NamedType(this, QualifiedName("vm_prot_t")), "initprot");
    loadSegmentCommand64Builder.AddMember(Type::IntegerType(4, false), "nsects");
    loadSegmentCommand64Builder.AddMember(Type::NamedType(this, QualifiedName("sg_flags_t")), "flags");
    Ref<Structure> loadSegmentCommand64Struct = loadSegmentCommand64Builder.Finalize();
    QualifiedName loadSegment64CommandName = std::string("segment_command_64");
    std::string loadSegment64CommandTypeId = Type::GenerateAutoTypeId("macho", loadSegment64CommandName);
    Ref<Type> loadSegment64CommandType = Type::StructureType(loadSegmentCommand64Struct);
    DefineType(loadSegment64CommandTypeId, loadSegment64CommandName, loadSegment64CommandType);

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
    DefineType(sectionTypeId, sectionName, sectionType);

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
    DefineType(section64TypeId, section64Name, section64Type);

    StructureBuilder symtabBuilder;
    symtabBuilder.AddMember(Type::NamedType(this, QualifiedName("load_command_type_t")), "cmd");
    symtabBuilder.AddMember(Type::IntegerType(4, false), "cmdsize");
    symtabBuilder.AddMember(Type::IntegerType(4, false), "symoff");
    symtabBuilder.AddMember(Type::IntegerType(4, false), "nsyms");
    symtabBuilder.AddMember(Type::IntegerType(4, false), "stroff");
    symtabBuilder.AddMember(Type::IntegerType(4, false), "strsize");
    Ref<Structure> symtabStruct = symtabBuilder.Finalize();
    QualifiedName symtabName = std::string("symtab");
    std::string symtabTypeId = Type::GenerateAutoTypeId("macho", symtabName);
    Ref<Type> symtabType = Type::StructureType(symtabStruct);
    DefineType(symtabTypeId, symtabName, symtabType);

    StructureBuilder dynsymtabBuilder;
    dynsymtabBuilder.AddMember(Type::NamedType(this, QualifiedName("load_command_type_t")), "cmd");
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
    DefineType(dynsymtabTypeId, dynsymtabName, dynsymtabType);

    StructureBuilder uuidBuilder;
    uuidBuilder.AddMember(Type::NamedType(this, QualifiedName("load_command_type_t")), "cmd");
    uuidBuilder.AddMember(Type::IntegerType(4, false), "cmdsize");
    uuidBuilder.AddMember(Type::ArrayType(Type::IntegerType(1, false), 16), "uuid");
    Ref<Structure> uuidStruct = uuidBuilder.Finalize();
    QualifiedName uuidName = std::string("uuid");
    std::string uuidTypeId = Type::GenerateAutoTypeId("macho", uuidName);
    Ref<Type> uuidType = Type::StructureType(uuidStruct);
    DefineType(uuidTypeId, uuidName, uuidType);

    StructureBuilder linkeditDataBuilder;
    linkeditDataBuilder.AddMember(Type::NamedType(this, QualifiedName("load_command_type_t")), "cmd");
    linkeditDataBuilder.AddMember(Type::IntegerType(4, false), "cmdsize");
    linkeditDataBuilder.AddMember(Type::IntegerType(4, false), "dataoff");
    linkeditDataBuilder.AddMember(Type::IntegerType(4, false), "datasize");
    Ref<Structure> linkeditDataStruct = linkeditDataBuilder.Finalize();
    QualifiedName linkeditDataName = std::string("linkedit_data");
    std::string linkeditDataTypeId = Type::GenerateAutoTypeId("macho", linkeditDataName);
    Ref<Type> linkeditDataType = Type::StructureType(linkeditDataStruct);
    DefineType(linkeditDataTypeId, linkeditDataName, linkeditDataType);

    StructureBuilder encryptionInfoBuilder;
    encryptionInfoBuilder.AddMember(Type::NamedType(this, QualifiedName("load_command_type_t")), "cmd");
    encryptionInfoBuilder.AddMember(Type::IntegerType(4, false), "cmdsize");
    encryptionInfoBuilder.AddMember(Type::IntegerType(4, false), "cryptoff");
    encryptionInfoBuilder.AddMember(Type::IntegerType(4, false), "cryptsize");
    encryptionInfoBuilder.AddMember(Type::IntegerType(4, false), "cryptid");
    Ref<Structure> encryptionInfoStruct = encryptionInfoBuilder.Finalize();
    QualifiedName encryptionInfoName = std::string("encryption_info");
    std::string encryptionInfoTypeId = Type::GenerateAutoTypeId("macho", encryptionInfoName);
    Ref<Type> encryptionInfoType = Type::StructureType(encryptionInfoStruct);
    DefineType(encryptionInfoTypeId, encryptionInfoName, encryptionInfoType);

    StructureBuilder versionMinBuilder;
    versionMinBuilder.AddMember(Type::NamedType(this, QualifiedName("load_command_type_t")), "cmd");
    versionMinBuilder.AddMember(Type::IntegerType(4, false), "cmdsize");
    versionMinBuilder.AddMember(Type::IntegerType(4, false), "version");
    versionMinBuilder.AddMember(Type::IntegerType(4, false), "sdk");
    Ref<Structure> versionMinStruct = versionMinBuilder.Finalize();
    QualifiedName versionMinName = std::string("version_min");
    std::string versionMinTypeId = Type::GenerateAutoTypeId("macho", versionMinName);
    Ref<Type> versionMinType = Type::StructureType(versionMinStruct);
    DefineType(versionMinTypeId, versionMinName, versionMinType);

    StructureBuilder dyldInfoBuilder;
    dyldInfoBuilder.AddMember(Type::NamedType(this, QualifiedName("load_command_type_t")), "cmd");
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
    DefineType(dyldInfoTypeId, dyldInfoName, dyldInfoType);

    StructureBuilder dylibBuilder;
    dylibBuilder.AddMember(Type::IntegerType(4, false), "name");
    dylibBuilder.AddMember(Type::IntegerType(4, false), "timestamp");
    dylibBuilder.AddMember(Type::IntegerType(4, false), "current_version");
    dylibBuilder.AddMember(Type::IntegerType(4, false), "compatibility_version");
    Ref<Structure> dylibStruct = dylibBuilder.Finalize();
    QualifiedName dylibName = std::string("dylib");
    std::string dylibTypeId = Type::GenerateAutoTypeId("macho", dylibName);
    Ref<Type> dylibType = Type::StructureType(dylibStruct);
    DefineType(dylibTypeId, dylibName, dylibType);

    StructureBuilder dylibCommandBuilder;
    dylibCommandBuilder.AddMember(Type::NamedType(this, QualifiedName("load_command_type_t")), "cmd");
    dylibCommandBuilder.AddMember(Type::IntegerType(4, false), "cmdsize");
    dylibCommandBuilder.AddMember(Type::NamedType(this, QualifiedName("dylib")), "dylib");
    Ref<Structure> dylibCommandStruct = dylibCommandBuilder.Finalize();
    QualifiedName dylibCommandName = std::string("dylib_command");
    std::string dylibCommandTypeId = Type::GenerateAutoTypeId("macho", dylibCommandName);
    Ref<Type> dylibCommandType = Type::StructureType(dylibCommandStruct);
    DefineType(dylibCommandTypeId, dylibCommandName, dylibCommandType);

    StructureBuilder filesetEntryCommandBuilder;
    filesetEntryCommandBuilder.AddMember(Type::NamedType(this, QualifiedName("load_command_type_t")), "cmd");
    filesetEntryCommandBuilder.AddMember(Type::IntegerType(4, false), "cmdsize");
    filesetEntryCommandBuilder.AddMember(Type::IntegerType(8, false), "vmaddr");
    filesetEntryCommandBuilder.AddMember(Type::IntegerType(8, false), "fileoff");
    filesetEntryCommandBuilder.AddMember(Type::IntegerType(4, false), "entry_id");
    filesetEntryCommandBuilder.AddMember(Type::IntegerType(4, false), "reserved");
    Ref<Structure> filesetEntryCommandStruct = filesetEntryCommandBuilder.Finalize();
    QualifiedName filesetEntryCommandName = std::string("fileset_entry_command");
    std::string filesetEntryCommandTypeId = Type::GenerateAutoTypeId("macho", filesetEntryCommandName);
    Ref<Type> filesetEntryCommandType = Type::StructureType(filesetEntryCommandStruct);
    DefineType(filesetEntryCommandTypeId, filesetEntryCommandName, filesetEntryCommandType);

    std::vector<LoadedImage> images;
    if (auto meta = GetParentView()->GetParentView()->QueryMetadata(SharedCacheMetadataTag))
    {
        std::string data = GetParentView()->GetParentView()->GetStringMetadata(SharedCacheMetadataTag);
        BNLogError("%s", data.c_str());
        std::stringstream ss;
        ss.str(data);
        rapidjson::Document result(rapidjson::kObjectType);

        result.Parse(data.c_str());
        for (auto &imgV: result["loadedImages"].GetArray())
        {
            if (imgV.HasMember("name"))
            {
                auto name = imgV.FindMember("name");
                if (name != imgV.MemberEnd())
                    images.push_back(LoadedImage::deserialize(imgV.GetObject()));
            }
        }
    }
    else
    {
        auto reader = new BinaryReader(GetParentView());
        reader->Seek(16);
        auto size = reader->Read32();
        //WriteBuffer(0, GetParentView()->ReadBuffer(0, size));
        AddAutoSegment(0, size, 0, size, SegmentReadable);
        return true;
    }

    for (auto image : images)
    {
        for (auto seg : image.loadedSegments)
        {
            // yeah ok this sucks ass
            // but is literally the only way
            // we're in deser, we have to rebuild our parent view as well here
            GetParentView()->AddUserSegment(seg.first, seg.second.second, seg.first, seg.second.second, SegmentReadable);
            GetParentView()->WriteBuffer(seg.first, GetParentView()->GetParentView()->ReadBuffer(seg.first, seg.second.second));
            //AddAutoSegment(seg.second.first, seg.second.second, seg.first, seg.second.second, SegmentReadable | SegmentExecutable);
        }
    }

    return true;
}


DSCViewType::DSCViewType()
    : BinaryViewType("DSCView", "DSCView")
{
}

BinaryNinja::Ref<BinaryNinja::BinaryView> DSCViewType::Create(BinaryNinja::BinaryView *data)
{
    return new DSCView("DSCView", new DSCRawView("DSCRawView", data, false), false);
}

BinaryNinja::Ref<BinaryNinja::BinaryView> DSCViewType::Parse(BinaryNinja::BinaryView *data)
{
    return new DSCView("DSCView", new DSCRawView("DSCRawView", data, true), true);
}

bool DSCViewType::IsTypeValidForData(BinaryNinja::BinaryView *data)
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
    auto path = m_dscView->GetFile()->GetOriginalFilename();
    try {
        m_baseFile = std::shared_ptr<MMappedFileAccessor>(new MMappedFileAccessor(path));
    }
    catch (MissingFileException& exc)
    {
        return false;
    }

    DataBuffer sig = *m_baseFile->ReadBuffer(0, 4);
    if (sig.GetLength() != 4)
        return false;
    const char *magic = (char *) sig.GetData();
    if (strncmp(magic, "dyld", 4) == 0)
    {
    }
    else
        return false;
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

    rapidjson::Document d;
    d.SetObject();

    rapidjson::Document::AllocatorType& allocator = d.GetAllocator();

    d.AddMember("state", m_viewState, allocator);
    d.AddMember("cursor", m_rawViewCursor, allocator);
    rapidjson::Value loadedImages(rapidjson::kArrayType);
    for (auto img : m_loadedImages)
    {
        loadedImages.PushBack(img.second.serialize(allocator), allocator);
    }
    d.AddMember("loadedImages", loadedImages, allocator);

    rapidjson::StringBuffer strbuf;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(strbuf);
    d.Accept(writer);

    return strbuf.GetString();
}

void SharedCache::DeserializeFromRawView()
{
    if (m_dscView->QueryMetadata(SharedCacheMetadataTag))
    {
        std::string data = m_dscView->GetStringMetadata(SharedCacheMetadataTag);
        std::stringstream ss;
        ss.str(data);
        rapidjson::Document result(rapidjson::kObjectType);

        result.Parse(data.c_str());
        m_viewState = static_cast<ViewState>(result["state"].GetUint64());
        m_rawViewCursor = result["cursor"].GetUint64();
        for (auto &imgV: result["loadedImages"].GetArray())
        {
            if (imgV.HasMember("name"))
            {
                auto name = imgV.FindMember("name");
                if (name != imgV.MemberEnd())
                    m_loadedImages[name->value.GetString()] = LoadedImage::deserialize(imgV.GetObject());
            }
        }

    }
    else
    {
        m_viewState = Loaded;
        m_loadedImages.clear();
        m_rawViewCursor = m_dscView->GetParentView()->GetEnd();
    }
}

SharedCache::SharedCache(BinaryNinja::Ref<BinaryNinja::BinaryView> dscView)
    : m_dscView(dscView)
{
    DeserializeFromRawView();
}

SharedCache* SharedCache::GetFromDSCView(BinaryNinja::Ref<BinaryNinja::BinaryView> dscView)
{
    return new SharedCache(std::move(dscView));
}

bool SharedCache::LoadImageWithInstallName(std::string installName)
{
    auto mapLock = ScopedVMMapSession(this);
    if (!m_baseFile)
        return false;
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

    m_viewState = LoadedWithImages;
    m_rawViewCursor = m_dscView->GetParentView()->GetEnd();
    auto reader = VMReader(m_vm);
    reader.Seek(image.headerBase);
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
                if (cmd.vmsize >= 0x8000000)
                    continue;
                auto buff = reader.ReadBuffer(cmd.vmaddr, cmd.vmsize);
                // wow this sucks!
                m_dscView->GetParentView()->GetParentView()->WriteBuffer(m_dscView->GetParentView()->GetParentView()->GetEnd(), *buff);
                m_dscView->GetParentView()->WriteBuffer(m_rawViewCursor, *buff);
                image.loadedSegments.push_back({m_rawViewCursor, {cmd.vmaddr, cmd.vmaddr + cmd.vmsize}});
                m_dscView->GetParentView()->AddUserSegment(m_rawViewCursor, cmd.vmsize, m_rawViewCursor, cmd.vmsize, SegmentReadable);
                m_dscView->AddUserSegment(cmd.vmaddr, cmd.vmsize, m_rawViewCursor, cmd.vmsize, SegmentReadable | SegmentExecutable);
                m_dscView->WriteBuffer(cmd.vmaddr, *buff);
                m_rawViewCursor = m_dscView->GetParentView()->GetEnd();
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
                if (cmd.vmsize >= 0x8000000)
                    continue;
                auto buff = reader.ReadBuffer(cmd.vmaddr, cmd.vmsize);
                m_dscView->GetParentView()->WriteBuffer(m_rawViewCursor, *buff);
                image.loadedSegments.push_back({m_rawViewCursor, {cmd.vmaddr, cmd.vmaddr + cmd.vmsize}});
                m_dscView->GetParentView()->AddUserSegment(m_rawViewCursor, cmd.vmsize, m_rawViewCursor, cmd.vmsize, SegmentReadable);
                m_dscView->AddUserSegment(cmd.vmaddr, cmd.vmsize, m_rawViewCursor, cmd.vmsize, SegmentReadable | SegmentExecutable);
                m_dscView->WriteBuffer(cmd.vmaddr, *buff);
                m_rawViewCursor = m_dscView->GetParentView()->GetEnd();
            }
            off += bump;
        }
    }

    m_loadedImages[image.name] = image;
    SaveToDSCView();

    auto h = MachOLoader::HeaderForAddress(m_dscView, image.headerBase, image.name);
    MachOLoader::InitializeHeader(m_dscView, h);
    if (h.exportTriePresent)
        MachOLoader::ParseExportTrie(m_vm->MappingAtAddress(h.linkeditSegment.vmaddr).first.file.get(), m_dscView, h);

    return true;
}

std::string base_name(std::string const & path)
{
    return path.substr(path.find_last_of("/\\") + 1);
}


MachOLoader::MachOHeader MachOLoader::HeaderForAddress(Ref<BinaryView> data, uint64_t address, std::string identifierPrefix)
{
    MachOHeader header;

    header.textBase = address;
    header.identifierPrefix = base_name(identifierPrefix);
    header.stringList = new DataBuffer();

    std::string errorMsg;
    // address is a Raw file offset
    BinaryReader reader(data);
    reader.Seek(address);
    reader.SetVirtualBase(address);

    header.ident.magic = reader.Read32();

    BNEndianness endianness;
    if (header.ident.magic == MH_MAGIC || header.ident.magic == MH_MAGIC_64)
        endianness = LittleEndian;
    else if (header.ident.magic == MH_CIGAM || header.ident.magic == MH_CIGAM_64)
        endianness = BigEndian;
    else
    {
        throw ReadException();
    }

    reader.SetEndianness(endianness);
    header.ident.cputype    = reader.Read32();
    header.ident.cpusubtype = reader.Read32();
    header.ident.filetype   = reader.Read32();
    header.ident.ncmds      = reader.Read32();
    header.ident.sizeofcmds = reader.Read32();
    header.ident.flags      = reader.Read32();
    if ((header.ident.cputype & MachOABIMask) == MachOABI64) // address size == 8
    {
        header.ident.reserved = reader.Read32();
    }
    header.loadCommandOffset = reader.GetOffset();

    bool first = true;
    // Parse segment commands
    try
    {
        for (size_t i = 0; i < header.ident.ncmds; i++)
        {
            BNLogInfo("of 0x%llx", reader.GetOffset());
            load_command load;
            segment_command_64 segment64;
            section_64 sect;
            memset(&sect, 0, sizeof(sect));
            size_t curOffset = reader.GetOffset();
            load.cmd = reader.Read32();
            load.cmdsize = reader.Read32();
            size_t nextOffset = curOffset + load.cmdsize;
            if (load.cmdsize < sizeof(load_command))
                throw MachoFormatException("unable to read header");

            switch (load.cmd)
            {
                case LC_MAIN:
                {
                    uint64_t entryPoint = reader.Read64();
                    header.entryPoints.push_back({entryPoint, true});
                    (void)reader.Read64(); // Stack start
                    break;
                }
                case LC_SEGMENT: //map the 32bit version to 64 bits
                    segment64.cmd = LC_SEGMENT_64;
                    reader.Read(&segment64.segname, 16);
                    segment64.vmaddr = reader.Read32();
                    segment64.vmsize = reader.Read32();
                    segment64.fileoff = reader.Read32();
                    segment64.filesize = reader.Read32();
                    segment64.maxprot = reader.Read32();
                    segment64.initprot = reader.Read32();
                    segment64.nsects = reader.Read32();
                    segment64.flags = reader.Read32();
                    if (first)
                    {
                        if (!((header.ident.flags & MH_SPLIT_SEGS) || header.ident.cputype == MACHO_CPU_TYPE_X86_64)
                            || (segment64.flags & MACHO_VM_PROT_WRITE))
                        {
                            header.relocationBase = segment64.vmaddr;
                            first = false;
                        }
                    }
                    for (size_t j = 0; j < segment64.nsects; j++)
                    {
                        reader.Read(&sect.sectname, 16);
                        reader.Read(&sect.segname, 16);
                        sect.addr = reader.Read32();
                        sect.size = reader.Read32();
                        sect.offset = reader.Read32();
                        sect.align = reader.Read32();
                        sect.reloff = reader.Read32();
                        sect.nreloc = reader.Read32();
                        sect.flags = reader.Read32();
                        sect.reserved1 = reader.Read32();
                        sect.reserved2 = reader.Read32();
                        // if the segment isn't mapped into virtual memory don't add the corresponding sections.
                        if (segment64.vmsize > 0)
                        {
                            header.sections.push_back(sect);
                        }
                        if (!strncmp(sect.sectname, "__mod_init_func", 15))
                            header.moduleInitSections.push_back(sect);
                        if ((sect.flags & (S_ATTR_SELF_MODIFYING_CODE | S_SYMBOL_STUBS)) == (S_ATTR_SELF_MODIFYING_CODE | S_SYMBOL_STUBS))
                            header.symbolStubSections.push_back(sect);
                        if ((sect.flags & S_NON_LAZY_SYMBOL_POINTERS) == S_NON_LAZY_SYMBOL_POINTERS)
                            header.symbolPointerSections.push_back(sect);
                        if ((sect.flags & S_LAZY_SYMBOL_POINTERS) == S_LAZY_SYMBOL_POINTERS)
                            header.symbolPointerSections.push_back(sect);
                    }
                    header.segments.push_back(segment64);
                    break;
                case LC_SEGMENT_64:
                    segment64.cmd = LC_SEGMENT_64;
                    reader.Read(&segment64.segname, 16);
                    segment64.vmaddr = reader.Read64();
                    segment64.vmsize = reader.Read64();
                    segment64.fileoff = reader.Read64();
                    segment64.filesize = reader.Read64();
                    segment64.maxprot = reader.Read32();
                    segment64.initprot = reader.Read32();
                    segment64.nsects = reader.Read32();
                    segment64.flags = reader.Read32();
                    if (strncmp(segment64.segname, "__LINKEDIT", 10) == 0)
                        header.linkeditSegment = segment64;
                    if (first)
                    {
                        if (!((header.ident.flags & MH_SPLIT_SEGS) || header.ident.cputype == MACHO_CPU_TYPE_X86_64)
                            || (segment64.flags & MACHO_VM_PROT_WRITE))
                        {
                            header.relocationBase = segment64.vmaddr;
                            first = false;
                        }
                    }
                    for (size_t j = 0; j < segment64.nsects; j++)
                    {
                        reader.Read(&sect.sectname, 16);
                        reader.Read(&sect.segname, 16);
                        sect.addr = reader.Read64();
                        sect.size = reader.Read64();
                        sect.offset = reader.Read32();
                        sect.align = reader.Read32();
                        sect.reloff = reader.Read32();
                        sect.nreloc = reader.Read32();
                        sect.flags = reader.Read32();
                        sect.reserved1 = reader.Read32();
                        sect.reserved2 = reader.Read32();
                        sect.reserved3 = reader.Read32();
                        // if the segment isn't mapped into virtual memory don't add the corresponding sections.
                        if (segment64.vmsize > 0)
                        {
                            header.sections.push_back(sect);
                        }

                        if (!strncmp(sect.sectname, "__mod_init_func", 15))
                            header.moduleInitSections.push_back(sect);
                        if ((sect.flags & (S_ATTR_SELF_MODIFYING_CODE | S_SYMBOL_STUBS)) == (S_ATTR_SELF_MODIFYING_CODE | S_SYMBOL_STUBS))
                            header.symbolStubSections.push_back(sect);
                        if ((sect.flags & S_NON_LAZY_SYMBOL_POINTERS) == S_NON_LAZY_SYMBOL_POINTERS)
                            header.symbolPointerSections.push_back(sect);
                        if ((sect.flags & S_LAZY_SYMBOL_POINTERS) == S_LAZY_SYMBOL_POINTERS)
                            header.symbolPointerSections.push_back(sect);
                    }
                    header.segments.push_back(segment64);
                    break;
                case LC_ROUTINES: //map the 32bit version to 64bits
                    header.routines64.cmd = LC_ROUTINES_64;
                    header.routines64.init_address = reader.Read32();
                    header.routines64.init_module = reader.Read32();
                    header.routines64.reserved1 = reader.Read32();
                    header.routines64.reserved2 = reader.Read32();
                    header.routines64.reserved3 = reader.Read32();
                    header.routines64.reserved4 = reader.Read32();
                    header.routines64.reserved5 = reader.Read32();
                    header.routines64.reserved6 = reader.Read32();
                    header.routinesPresent = true;
                    break;
                case LC_ROUTINES_64:
                    header.routines64.cmd = LC_ROUTINES_64;
                    header.routines64.init_address = reader.Read64();
                    header.routines64.init_module = reader.Read64();
                    header.routines64.reserved1 = reader.Read64();
                    header.routines64.reserved2 = reader.Read64();
                    header.routines64.reserved3 = reader.Read64();
                    header.routines64.reserved4 = reader.Read64();
                    header.routines64.reserved5 = reader.Read64();
                    header.routines64.reserved6 = reader.Read64();
                    header.routinesPresent = true;
                    break;
                case LC_FUNCTION_STARTS:
                    header.functionStarts.funcoff = reader.Read32();
                    header.functionStarts.funcsize = reader.Read32();
                    header.functionStartsPresent = true;
                    break;
                case LC_SYMTAB:
                    header.symtab.symoff  = reader.Read32();
                    header.symtab.nsyms   = reader.Read32();
                    header.symtab.stroff  = reader.Read32();
                    header.symtab.strsize = reader.Read32();
                    //reader.Seek(header.symtab.stroff);
                    //header.stringList->Append(reader.Read(header.symtab.strsize));
                    header.stringListSize = header.symtab.strsize;
                    break;
                case LC_DYSYMTAB:
                    header.dysymtab.ilocalsym = reader.Read32();
                    header.dysymtab.nlocalsym = reader.Read32();
                    header.dysymtab.iextdefsym = reader.Read32();
                    header.dysymtab.nextdefsym = reader.Read32();
                    header.dysymtab.iundefsym = reader.Read32();
                    header.dysymtab.nundefsym = reader.Read32();
                    header.dysymtab.tocoff = reader.Read32();
                    header.dysymtab.ntoc = reader.Read32();
                    header.dysymtab.modtaboff = reader.Read32();
                    header.dysymtab.nmodtab = reader.Read32();
                    header.dysymtab.extrefsymoff = reader.Read32();
                    header.dysymtab.nextrefsyms = reader.Read32();
                    header.dysymtab.indirectsymoff = reader.Read32();
                    header.dysymtab.nindirectsyms = reader.Read32();
                    header.dysymtab.extreloff = reader.Read32();
                    header.dysymtab.nextrel = reader.Read32();
                    header.dysymtab.locreloff = reader.Read32();
                    header.dysymtab.nlocrel = reader.Read32();
                    header.dysymPresent = true;
                    break;
                case LC_DYLD_CHAINED_FIXUPS:
                    header.chainedFixups.dataoff = reader.Read32();
                    header.chainedFixups.datasize = reader.Read32();
                    header.chainedFixupsPresent = true;
                    break;
                case LC_DYLD_INFO:
                case LC_DYLD_INFO_ONLY:
                    header.dyldInfo.rebase_off = reader.Read32();
                    header.dyldInfo.rebase_size = reader.Read32();
                    header.dyldInfo.bind_off = reader.Read32();
                    header.dyldInfo.bind_size = reader.Read32();
                    header.dyldInfo.weak_bind_off = reader.Read32();
                    header.dyldInfo.weak_bind_size = reader.Read32();
                    header.dyldInfo.lazy_bind_off = reader.Read32();
                    header.dyldInfo.lazy_bind_size = reader.Read32();
                    header.dyldInfo.export_off = reader.Read32();
                    header.dyldInfo.export_size = reader.Read32();
                    header.exportTrie.dataoff = header.dyldInfo.export_off;
                    header.exportTrie.datasize = header.dyldInfo.export_size;
                    header.exportTriePresent = true;
                    header.dyldInfoPresent = true;
                    break;
                case LC_DYLD_EXPORTS_TRIE:
                    header.exportTrie.dataoff = reader.Read32();
                    header.exportTrie.datasize = reader.Read32();
                    header.exportTriePresent = true;
                    break;
                case LC_THREAD:
                case LC_UNIXTHREAD:
                    /*while (reader.GetOffset() < nextOffset)
                    {

                        thread_command thread;
                        thread.flavor = reader.Read32();
                        thread.count = reader.Read32();
                        switch (m_archId)
                        {
                            case MachOx64:
                                m_logger->LogDebug("x86_64 Thread state\n");
                                if (thread.flavor != X86_THREAD_STATE64)
                                {
                                    reader.SeekRelative(thread.count * sizeof(uint32_t));
                                    break;
                                }
                                //This wont be big endian so we can just read the whole thing
                                reader.Read(&thread.statex64, sizeof(thread.statex64));
                                header.entryPoints.push_back({thread.statex64.rip, false});
                                break;
                            case MachOx86:
                                m_logger->LogDebug("x86 Thread state\n");
                                if (thread.flavor != X86_THREAD_STATE32)
                                {
                                    reader.SeekRelative(thread.count * sizeof(uint32_t));
                                    break;
                                }
                                //This wont be big endian so we can just read the whole thing
                                reader.Read(&thread.statex86, sizeof(thread.statex86));
                                header.entryPoints.push_back({thread.statex86.eip, false});
                                break;
                            case MachOArm:
                                m_logger->LogDebug("Arm Thread state\n");
                                if (thread.flavor != _ARM_THREAD_STATE)
                                {
                                    reader.SeekRelative(thread.count * sizeof(uint32_t));
                                    break;
                                }
                                //This wont be big endian so we can just read the whole thing
                                reader.Read(&thread.statearmv7, sizeof(thread.statearmv7));
                                header.entryPoints.push_back({thread.statearmv7.r15, false});
                                break;
                            case MachOAarch64:
                            case MachOAarch6432:
                                m_logger->LogDebug("Aarch64 Thread state\n");
                                if (thread.flavor != _ARM_THREAD_STATE64)
                                {
                                    reader.SeekRelative(thread.count * sizeof(uint32_t));
                                    break;
                                }
                                reader.Read(&thread.stateaarch64, sizeof(thread.stateaarch64));
                                header.entryPoints.push_back({thread.stateaarch64.pc, false});
                                break;
                            case MachOPPC:
                                m_logger->LogDebug("PPC Thread state\n");
                                if (thread.flavor != PPC_THREAD_STATE)
                                {
                                    reader.SeekRelative(thread.count * sizeof(uint32_t));
                                    break;
                                }
                                //Read individual entries for endian reasons
                                header.entryPoints.push_back({reader.Read32(), false});
                                (void)reader.Read32();
                                (void)reader.Read32();
                                //Read the rest of the structure
                                (void)reader.Read(&thread.stateppc.r1, sizeof(thread.stateppc) - (3 * 4));
                                break;
                            case MachOPPC64:
                                m_logger->LogDebug("PPC64 Thread state\n");
                                if (thread.flavor != PPC_THREAD_STATE64)
                                {
                                    reader.SeekRelative(thread.count * sizeof(uint32_t));
                                    break;
                                }
                                header.entryPoints.push_back({reader.Read64(), false});
                                (void)reader.Read64();
                                (void)reader.Read64(); // Stack start
                                (void)reader.Read(&thread.stateppc64.r1, sizeof(thread.stateppc64) - (3 * 8));
                                break;
                            default:
                                m_logger->LogError("Unknown archid: %x", m_archId);
                        }

                    }*/
                    break;
                case LC_LOAD_DYLIB:
                {
                    uint32_t offset = reader.Read32();
                    if (offset < nextOffset)
                    {
                        reader.Seek(curOffset + offset);
                        std::string libname = reader.ReadCString();
                        header.dylibs.push_back(libname);
                    }
                }
                    break;
                case LC_BUILD_VERSION:
                {
                    //m_logger->LogDebug("LC_BUILD_VERSION:");
                    header.buildVersion.platform = reader.Read32();
                    header.buildVersion.minos = reader.Read32();
                    header.buildVersion.sdk = reader.Read32();
                    header.buildVersion.ntools = reader.Read32();
                    //m_logger->LogDebug("Platform: %s", BuildPlatformToString(header.buildVersion.platform).c_str());
                    //m_logger->LogDebug("MinOS: %s", BuildToolVersionToString(header.buildVersion.minos).c_str());
                    //m_logger->LogDebug("SDK: %s", BuildToolVersionToString(header.buildVersion.sdk).c_str());
                    for (uint32_t i = 0; (i < header.buildVersion.ntools) && (i < 10); i++)
                    {
                        uint32_t tool = reader.Read32();
                        uint32_t version = reader.Read32();
                        header.buildToolVersions.push_back({tool, version});
                        //m_logger->LogDebug("Build Tool: %s: %s", BuildToolToString(tool).c_str(), BuildToolVersionToString(version).c_str());
                    }
                    break;
                }
                case LC_FILESET_ENTRY:
                {
                    throw ReadException(); // huh
                    break;
                }
                default:
                   // m_logger->LogDebug("Unhandled command: %s : %" PRIu32 "\n", CommandToString(load.cmd).c_str(), load.cmdsize);
                    break;
            }
            if (reader.GetOffset() != nextOffset)
            {
                // m_logger->LogDebug("Didn't parse load command: %s fully %" PRIx64 ":%" PRIxPTR, CommandToString(load.cmd).c_str(), reader.GetOffset(), nextOffset);
            }
            reader.Seek(nextOffset);
        }
    }
    catch (ReadException&)
    {
        throw MachoFormatException("Mach-O section headers invalid");
    }

    return header;
}

void MachOLoader::InitializeHeader(Ref<BinaryView> view, MachOLoader::MachOHeader header)
{
    for (auto& section : header.sections)
    {
        char sectionName[17];
        memcpy(sectionName, section.sectname, sizeof(section.sectname));
        sectionName[16] = 0;
        if (header.identifierPrefix.empty())
            header.sectionNames.push_back(sectionName);
        else
            header.sectionNames.push_back(header.identifierPrefix + "::" + sectionName);
    }

    for (size_t i = 0; i < header.sections.size(); i++)
    {
        if (!header.sections[i].size)
            continue;

        std::string type;
        BNSectionSemantics semantics = DefaultSectionSemantics;
        switch (header.sections[i].flags & 0xff)
        {
            case S_REGULAR:
                if (header.sections[i].flags & S_ATTR_PURE_INSTRUCTIONS)
                {
                    type = "PURE_CODE";
                    semantics = ReadOnlyCodeSectionSemantics;
                }
                else if (header.sections[i].flags & S_ATTR_SOME_INSTRUCTIONS)
                {
                    type = "CODE";
                    semantics = ReadOnlyCodeSectionSemantics;
                }
                else
                {
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
        if (i >= header.sectionNames.size())
            break;
        if (strncmp(header.sections[i].sectname, "__text", sizeof(header.sections[i].sectname)) == 0)
            semantics = ReadOnlyCodeSectionSemantics;
        if (strncmp(header.sections[i].sectname, "__const", sizeof(header.sections[i].sectname)) == 0)
            semantics = ReadOnlyDataSectionSemantics;
        if (strncmp(header.sections[i].sectname, "__data", sizeof(header.sections[i].sectname)) == 0)
            semantics = ReadWriteDataSectionSemantics;
        if (strncmp(header.sections[i].segname, "__DATA_CONST", sizeof(header.sections[i].segname)) == 0)
            semantics = ReadOnlyDataSectionSemantics;

        view->AddUserSection(header.sectionNames[i], header.sections[i].addr, header.sections[i].size, semantics, type, header.sections[i].align);
    }

    BinaryReader virtualReader(view);
    view->DefineDataVariable(header.textBase, Type::NamedType(view, QualifiedName("mach_header_64")));
    view->DefineUserSymbol(new Symbol(DataSymbol, "__macho_header::" + header.identifierPrefix, header.textBase, LocalBinding));

    try
    {
        virtualReader.Seek(header.textBase + sizeof(mach_header_64));
        size_t sectionNum = 0;
        for (size_t i = 0; i < header.ident.ncmds; i++)
        {
            load_command load;
            uint64_t curOffset = virtualReader.GetOffset();
            load.cmd = virtualReader.Read32();
            load.cmdsize = virtualReader.Read32();
            uint64_t nextOffset = curOffset + load.cmdsize;
            switch (load.cmd)
            {
                case LC_SEGMENT:
                {
                    view->DefineDataVariable(curOffset, Type::NamedType(view, QualifiedName("segment_command")));
                    virtualReader.SeekRelative(5 * 8);
                    size_t numSections = virtualReader.Read32();
                    virtualReader.SeekRelative(4);
                    for (size_t j = 0; j < numSections; j++)
                    {
                        view->DefineDataVariable(virtualReader.GetOffset(), Type::NamedType(view, QualifiedName("section")));
                        view->DefineUserSymbol(new Symbol(DataSymbol, "__macho_section::" + header.identifierPrefix + "_[" + std::to_string(sectionNum++) + "]", virtualReader.GetOffset(), LocalBinding));
                        virtualReader.SeekRelative((8 * 8) + 4);
                    }
                    break;
                }
                case LC_SEGMENT_64:
                {
                    view->DefineDataVariable(curOffset, Type::NamedType(view, QualifiedName("segment_command_64")));
                    virtualReader.SeekRelative(7 * 8);
                    size_t numSections = virtualReader.Read32();
                    virtualReader.SeekRelative(4);
                    for (size_t j = 0; j < numSections; j++)
                    {
                        view->DefineDataVariable(virtualReader.GetOffset(), Type::NamedType(view, QualifiedName("section_64")));
                        view->DefineUserSymbol(new Symbol(DataSymbol, "__macho_section_64::" + header.identifierPrefix + "_[" + std::to_string(sectionNum++) + "]", virtualReader.GetOffset(), LocalBinding));
                        virtualReader.SeekRelative(10 * 8);
                    }
                    break;
                }
                case LC_SYMTAB:
                    view->DefineDataVariable(curOffset, Type::NamedType(view, QualifiedName("symtab")));
                    break;
                case LC_DYSYMTAB:
                    view->DefineDataVariable(curOffset, Type::NamedType(view, QualifiedName("dysymtab")));
                    break;
                case LC_UUID:
                    view->DefineDataVariable(curOffset, Type::NamedType(view, QualifiedName("uuid")));
                    break;
                case LC_ID_DYLIB:
                case LC_LOAD_DYLIB:
                case LC_REEXPORT_DYLIB:
                case LC_LOAD_WEAK_DYLIB:
                case LC_LOAD_UPWARD_DYLIB:
                    view->DefineDataVariable(curOffset, Type::NamedType(view, QualifiedName("dylib_command")));
                    if (load.cmdsize-24 <= 150)
                        view->DefineDataVariable(curOffset + 24, Type::ArrayType(Type::IntegerType(1, true), load.cmdsize-24));
                    break;
                case LC_CODE_SIGNATURE:
                case LC_SEGMENT_SPLIT_INFO:
                case LC_FUNCTION_STARTS:
                case LC_DATA_IN_CODE:
                case LC_DYLIB_CODE_SIGN_DRS:
                case LC_DYLD_EXPORTS_TRIE:
                case LC_DYLD_CHAINED_FIXUPS:
                    view->DefineDataVariable(curOffset, Type::NamedType(view, QualifiedName("linkedit_data")));
                    break;
                case LC_ENCRYPTION_INFO:
                    view->DefineDataVariable(curOffset, Type::NamedType(view, QualifiedName("encryption_info")));
                    break;
                case LC_VERSION_MIN_MACOSX:
                case LC_VERSION_MIN_IPHONEOS:
                    view->DefineDataVariable(curOffset, Type::NamedType(view, QualifiedName("version_min")));
                    break;
                case LC_DYLD_INFO:
                case LC_DYLD_INFO_ONLY:
                    view->DefineDataVariable(curOffset, Type::NamedType(view, QualifiedName("dyld_info")));
                    break;
                default:
                    view->DefineDataVariable(curOffset, Type::NamedType(view, QualifiedName("load_command")));
                    break;
            }

            view->DefineUserSymbol(new Symbol(DataSymbol, "__macho_load_command::" + header.identifierPrefix + "_[" + std::to_string(i) + "]", curOffset, LocalBinding));
            virtualReader.Seek(nextOffset);
        }
    }
    catch (ReadException&)
    {
        LogError("Error when applying Mach-O header types at %" PRIx64, header.textBase);
    }
}

std::vector<ExportTrieEntryStart> ReadExportNode(DataBuffer& buffer, std::vector<ExportNode>& results, const std::string& currentText, size_t cursor, uint32_t endGuard)
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

void MachOLoader::ParseExportTrie(MMappedFileAccessor* linkeditFile, Ref<BinaryView> view, MachOLoader::MachOHeader header)
{
    try {
        MMappedFileAccessor *reader = linkeditFile;

        std::string startText = "";
        std::vector<ExportNode> nodes;
        DataBuffer* buffer = reader->ReadBuffer(header.exportTrie.dataoff, header.exportTrie.datasize);
        std::deque<ExportTrieEntryStart> entries;
        entries.push_back({"", 0});
        while (!entries.empty())
        {
            for (std::deque<ExportTrieEntryStart>::iterator it = entries.begin(); it != entries.end();)
            {
                ExportTrieEntryStart entry = *it;
                it = entries.erase(it);

                for (const auto& newEntry : ReadExportNode(*buffer, nodes, entry.currentText,
                                                           entry.cursorPosition, header.exportTrie.datasize))
                    entries.push_back(newEntry);
            }
        }

        for (const auto &n: nodes) {
            if (!n.text.empty() && n.offset) {
                uint32_t flags;
                BNSymbolType type = DataSymbol;
                auto found = false;
                for (auto s: header.sections) {
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
                // view->DefineMachoSymbol(type, n.text, header.textBase + n.offset, NoBinding, false);
                BNLogInfo("0x%llx %s", header.textBase + n.offset, n.text.c_str());
                view->DefineUserSymbol(new Symbol(DataSymbol, n.text, header.textBase + n.offset));
            }
        }
    } catch (std::exception &e) {
        BNLogError("Failed to load Export Trie");
    }
}

std::vector<std::string> SharedCache::GetAvailableImages()
{
    std::vector<std::string> installNames;

    auto mapLock = ScopedVMMapSession(this);
    auto format = GetCacheFormat();

    if (!m_baseFile)
        return {};
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

bool BNDSCViewLoadImageWithInstallName(BNBinaryView* view, char* name)
{
    std::string imageName = std::string(name);
    BNFreeString(name);
    auto rawView = new BinaryView(view);

    if (auto cache = SharedCache::GetFromDSCView(rawView))
    {
        return cache->LoadImageWithInstallName(imageName);
    }

    return false;
}

char **BNDSCViewGetInstallNames(BNBinaryView *view, size_t *count)
{
    auto rawView = new BinaryView(view);

    if (auto cache = SharedCache::GetFromDSCView(rawView))
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
    static DSCViewType type;
    BinaryViewType::Register(&type);
    g_dscViewType = &type;
    g_dscRawViewType = &rawType;

    PluginCommand::Register("List Images", "List Images", [](BinaryView* view){
        auto cache = KAPI::SharedCache(view);
        for (const auto& s : cache.GetAvailableImages())
        {
            BNLogInfo("a %s", s.c_str());
        }
    });
}

