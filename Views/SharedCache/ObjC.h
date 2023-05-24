//
// Created by kat on 5/23/23.
//

#ifndef KSUITE_OBJC_H
#define KSUITE_OBJC_H

#include <binaryninjaapi.h>
#include "VM.h"
#include "SharedCache.h"

using namespace BinaryNinja;

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
    BinaryNinja::QualifiedName TaggedPointer;
    BinaryNinja::QualifiedName FastPointer;
    BinaryNinja::QualifiedName RelativePointer;

    BinaryNinja::QualifiedName ID;
    BinaryNinja::QualifiedName Selector;

    BinaryNinja::QualifiedName CFString;

    BinaryNinja::QualifiedName MethodList;
    BinaryNinja::QualifiedName Method;
    BinaryNinja::QualifiedName MethodListEntry;
    BinaryNinja::QualifiedName Class;
    BinaryNinja::QualifiedName ClassRO;
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

class MachOLoader;
class DSCView;
class SharedCache;

class ObjCProcessing {
    std::shared_ptr<VM> m_vm;
    Ref<BinaryView> m_dscView;
    SharedCache* m_cache;
    ObjCTypes m_types{};
    Ref<Logger> m_logger;

    std::mutex m_typeDefMutex;
    bool m_typesLoaded;

    std::optional<uint64_t> m_customRelativeMethodSelectorBase;

    void LoadTypes();

    std::vector<DSCObjC::Class *> GetClassList(KMachOHeader &image);

    std::vector<DSCObjC::Method> LoadMethodList(uint64_t addr);

    void ApplyMethodType(std::string className, std::string sel, std::string types, uint64_t imp);

public:
    ObjCProcessing(Ref<BinaryView> view, SharedCache *cache, std::shared_ptr<VM> vm);

    /*!
     * This attempts to replicate (some of) the processing done by workflow_objc
     *
     * Specifically, it handles anything that can be done pre-analysis.
     *
     * @param image MachOImage returned from MachOProcessor
     */
    void LoadObjCMetadata(KMachOHeader &image);
};
#endif //KSUITE_OBJC_H
