//
// Created by kat on 5/23/23.
//

#ifndef KSUITE_VM_H
#define KSUITE_VM_H
#include <binaryninjaapi.h>


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

#endif //KSUITE_VM_H
