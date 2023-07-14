//
// Created by kat on 5/19/23.
//

#include "SharedCache.h"
#include <binaryninjaapi.h>
#include <ksuiteapi.h>
#include "highlevelilinstruction.h"
#include "ObjC.h"
#include <filesystem>
#include <utility>
#include <sys/mman.h>
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
    if (strncmp(magic, "dyld", 4) != 0)
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
        loadedImages.PushBack(img.second.AsDocument(), allocator);
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
        LoadFromString(m_dscView->GetStringMetadata(SharedCacheMetadataTag));
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

uint64_t SharedCache::GetImageStart(std::string installName)
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

    return image.headerBase;
}

bool SharedCache::LoadSectionAtAddress(uint64_t address)
{
    SetupVMMap();
    if (!m_baseFile)
    {
        TeardownVMMap();
        return false;
    }
    auto vmhold = m_vm;
    auto format = GetCacheFormat();
    LoadedImage image;
    image.headerBase = 0;

    dyld_cache_header header{};
    size_t header_size = m_baseFile->ReadUInt32(16);
    m_baseFile->Read(&header, 0, std::min(header_size, sizeof(dyld_cache_header)));
    BinaryNinja::segment_command_64 seg;
    bool found = false;

    switch (format) {
        case RegularCacheFormat: {
            dyld_cache_image_info img{};

            for (size_t i = 0; i < header.imagesCountOld; i++) {
                m_baseFile->Read(&img, header.imagesOffsetOld + (i * sizeof(img)), sizeof(img));
                auto iname = m_baseFile->ReadNullTermString(img.pathFileOffset);
                BinaryNinja::mach_header_64 hdr;
                m_vm->Read(&hdr, img.address, sizeof(BinaryNinja::mach_header_64));
                uint64_t cursor = img.address + sizeof(BinaryNinja::mach_header_64);
                for (auto j = 0; j < hdr.ncmds; j++)
                {
                    uint32_t cmdI = m_vm->ReadUInt32(cursor);
                    uint32_t cmdS = m_vm->ReadUInt32(cursor+4);
                    uint32_t next = cursor + cmdS;
                    if (cmdI == LC_SEGMENT_64)
                    {
                        m_vm->Read(&seg, cursor, sizeof(BinaryNinja::segment_command_64));
                        if ( seg.vmaddr <= address && seg.vmaddr + seg.vmsize > address )
                        {
                            found = true;
                            image.headerBase = img.address;
                            image.name = iname;
                            break;
                        }
                    }
                    cursor = next;
                }
                if (found)
                    break;
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
                BinaryNinja::mach_header_64 hdr;
                m_vm->Read(&hdr, img.address, sizeof(BinaryNinja::mach_header_64));
                uint64_t cursor = img.address + sizeof(BinaryNinja::mach_header_64);
                for (auto j = 0; j < hdr.ncmds; j++)
                {
                    uint32_t cmdI = m_vm->ReadUInt32(cursor);
                    uint32_t cmdS = m_vm->ReadUInt32(cursor+4);
                    uint64_t next = cursor + cmdS;
                    if (cmdI == LC_SEGMENT_64)
                    {
                        m_vm->Read(&seg, cursor, sizeof(BinaryNinja::segment_command_64));
                        if ( seg.vmaddr <= address && seg.vmaddr + seg.vmsize > address )
                        {
                            found = true;
                            image.headerBase = img.address;
                            image.name = iname;
                            break;
                        }
                    }
                    cursor = next;
                }
                if (found)
                    break;
            }

            break;
        }
    }

    if (!found)
    {
        BNLogInfo("Addr 0x%llx not found", address);
        TeardownVMMap();
        return false;
    }

    if (!image.headerBase)
    {
        TeardownVMMap();
        return false;
    }

    auto id = m_dscView->BeginUndoActions();
    m_rawViewCursor = m_dscView->GetParentView()->GetEnd();
    auto reader = VMReader(m_vm);
    reader.Seek(image.headerBase);
    size_t headerStart = reader.Offset();
    auto magic = reader.ReadUInt32(headerStart);
    bool is64 = (magic == MH_MAGIC_64 || magic == MH_CIGAM_64);

    if (is64) {
        auto buff = reader.ReadBuffer(seg.vmaddr, seg.vmsize);
        // wow this sucks!
        m_dscView->GetParentView()->GetParentView()->WriteBuffer(m_dscView->GetParentView()->GetParentView()->GetEnd(), *buff);
        m_dscView->GetParentView()->WriteBuffer(m_rawViewCursor, *buff);
        m_dscView->GetParentView()->AddUserSegment(m_rawViewCursor, seg.vmsize, m_rawViewCursor, seg.vmsize, SegmentReadable);
        m_dscView->AddUserSegment(seg.vmaddr, seg.vmsize, m_rawViewCursor, seg.vmsize, SegmentReadable | SegmentExecutable);
        m_dscView->WriteBuffer(seg.vmaddr, *buff);
        m_rawViewCursor = m_dscView->GetParentView()->GetEnd();
    } else
    {

    }

    SaveToDSCView();

    auto h = MachOLoader::HeaderForAddress(m_dscView, image.headerBase, image.name);
    MachOLoader::InitializeHeader(m_dscView, h, address);
    if (h.exportTriePresent)
        MachOLoader::ParseExportTrie(m_vm->MappingAtAddress(h.linkeditSegment.vmaddr).first.file.get(), m_dscView, h);

    m_dscView->AddAnalysisOption("linearsweep");
    m_dscView->UpdateAnalysis();
    TeardownVMMap();

    m_dscView->CommitUndoActions(id);

    return true;
}

bool SharedCache::LoadImageWithInstallName(std::string installName)
{
    SetupVMMap();
    if (!m_baseFile)
    {
        TeardownVMMap();
        return false;
    }
    auto vmhold = m_vm;
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
    {
        TeardownVMMap();
        return false;
    }

    auto id = m_dscView->BeginUndoActions();
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

    if (m_loadedImages.empty())
    {
        auto seg = m_dscView->GetSegmentAt(0);
        if (seg)
            m_dscView->RemoveAutoSegment(0, seg->GetLength());
    }

    m_loadedImages[image.name] = image;
    SaveToDSCView();

    auto h = MachOLoader::HeaderForAddress(m_dscView, image.headerBase, image.name);
    MachOLoader::InitializeHeader(m_dscView, h);
    if (h.exportTriePresent)
        MachOLoader::ParseExportTrie(m_vm->MappingAtAddress(h.linkeditSegment.vmaddr).first.file.get(), m_dscView, h);

    auto objc = new ObjCProcessing(m_dscView, this, m_vm);
    objc->LoadObjCMetadata(h);

    m_dscView->AddAnalysisOption("linearsweep");
    m_dscView->UpdateAnalysis();
    TeardownVMMap();

    m_dscView->CommitUndoActions(id);

    return true;
}

std::string base_name(std::string const & path)
{
    return path.substr(path.find_last_of("/\\") + 1);
}


KMachOHeader MachOLoader::HeaderForAddress(Ref<BinaryView> data, uint64_t address, std::string identifierPrefix)
{
    KMachOHeader header;

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

void MachOLoader::InitializeHeader(Ref<BinaryView> view, KMachOHeader header, uint64_t loadOnlySectionWithAddress)
{
    bool onlyLoadSingleSegment = loadOnlySectionWithAddress != 0;
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

        if (onlyLoadSingleSegment)
        {
            if (header.sections[i].addr <= loadOnlySectionWithAddress
            && header.sections[i].addr + header.sections[i].size > loadOnlySectionWithAddress)
            {
                view->AddUserSection(header.sectionNames[i], header.sections[i].addr, header.sections[i].size, semantics, type, header.sections[i].align);
                break;
            }
        }
        else
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

void MachOLoader::ParseExportTrie(MMappedFileAccessor* linkeditFile, Ref<BinaryView> view, KMachOHeader header)
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

bool BNDSCViewLoadSectionAtAddress(BNBinaryView* view, uint64_t addr)
{
    auto rawView = new BinaryView(view);

    if (auto cache = SharedCache::GetFromDSCView(rawView))
    {
        return cache->LoadSectionAtAddress(addr);
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

uint64_t BNDSCViewLoadedImageCount(BNBinaryView *view)
{

    auto rawView = new BinaryView(view);

    if (auto cache = SharedCache::GetFromDSCView(rawView))
    {
        return cache->LoadedImages().size();
    }

    return 0;
}
}

DSCViewType *g_dscViewType;
DSCRawViewType *g_dscRawViewType;

#ifdef BUILD_SHAREDCACHE

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

    PluginCommand::RegisterForAddress("Load Section At Address", "Load Section At Address",
                                      [](BinaryView* view, uint64_t addr)
    {
        auto cache = KAPI::SharedCache(view);
        uint64_t result;
        GetAddressInput(result, "Address", "Address");
        cache.LoadSectionAtAddress(result);
    },
    [](BinaryView* view, uint64_t addr)
    {
        return true;
    });
}

#endif

