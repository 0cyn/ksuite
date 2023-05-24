//
// Created by kat on 5/23/23.
//

#include "VM.h"
#include <filesystem>
#include <utility>
#include <csignal>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <memory>



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

std::pair<PageMapping, size_t> VM::MappingAtAddress(size_t address) {
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
