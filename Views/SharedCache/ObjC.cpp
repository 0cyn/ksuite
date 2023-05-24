//
// Created by kat on 5/23/23.
// this is a super trimmed down legacy version of objninja that really needs updated.
//

#include "ObjC.h"

std::pair<QualifiedName, Ref<Type>> FinalizeStructureBuilder(Ref<BinaryView> bv, StructureBuilder sb, std::string name) {
    auto classTypeStruct = sb.Finalize();

    QualifiedName classTypeName(name);
    auto classTypeId = Type::GenerateAutoTypeId("objc", classTypeName);
    auto classType = Type::StructureType(classTypeStruct);
    auto classQualName = bv->DefineType(classTypeId, classTypeName, classType);

    return {classQualName, classType};
}

inline void DefineTypedef(Ref<BinaryView> bv, const QualifiedName name, Ref<Type> type) {
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

ObjCProcessing::ObjCProcessing(Ref<BinaryView> view, SharedCache *cache, std::shared_ptr<VM> vm) : m_cache(cache), m_dscView(view), m_vm(vm) {
    m_logger = new Logger("objcLoader");
    m_typesLoaded = false;
    m_customRelativeMethodSelectorBase = std::nullopt;

    auto *reader = new VMReader(vm);

    if (auto addr = m_cache->GetImageStart("/usr/lib/libobjc.A.dylib")) {
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


std::vector<DSCObjC::Class *> ObjCProcessing::GetClassList(KMachOHeader &image) {
    std::vector<DSCObjC::Class *> classes{};
    VMReader *reader = new VMReader(m_vm);
    std::vector<uint64_t> classPtrs;

    if (auto cl = m_dscView->GetSectionByName(image.identifierPrefix + "::__objc_classlist")) {
        //// BNLogInfo("0x%llx", cl->second.addr);
        reader->Seek(cl->GetStart());
        size_t end = cl->GetStart() + cl->GetLength();
        for (size_t i = cl->GetStart(); i < end; i += 8) {
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
    auto *reader = new VMReader(m_vm);
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
            auto roff = reader->Offset();
            meth.name = reader->ReadULong(meth.name) & 0x1ffffffff;
            reader->Seek(roff);
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
    auto ok = m_dscView->ParseTypesFromSource(typeString, {}, {}, tpResult, errors);
    if (ok && !tpResult.functions.empty()) {
        auto functionType = tpResult.functions[0].type;

        // k: we are not in workflow phase so we need to define this here ourself.
        m_dscView->AddFunctionForAnalysis(m_dscView->GetDefaultPlatform(), imp);

        // Search for the method's implementation function; apply the type if found.
        if (auto f = m_dscView->GetAnalysisFunction(m_dscView->GetDefaultPlatform(), imp))
            f->SetUserType(functionType);
    }

    // TODO: Use '+' or '-' conditionally once class methods are supported. For
    // right now, only instance methods are analyzed and we can just use '-'.
    auto name = "-[" + className + " " + sel + "]";
    m_dscView->DefineUserSymbol(new Symbol(FunctionSymbol, name, imp));
}


void ObjCProcessing::LoadObjCMetadata(KMachOHeader &image) {
    if (!m_typesLoaded)
        LoadTypes();
    auto classes = GetClassList(image);

    for (auto c: classes) {
        //// BNLogInfo("%s", m_cache->m_vm->ReadNullTermString(c->ro_data->name).c_str());
        try {

            auto meths = LoadMethodList(c->ro_data->methods);

            for (auto m: meths) {
                //// BNLogInfo(":: %s", m_cache->m_vm->ReadNullTermString(m.name).c_str());
                ApplyMethodType(m_vm->ReadNullTermString(c->ro_data->name),
                                m_vm->ReadNullTermString(m.name), m_vm->ReadNullTermString(m.types),
                                m.imp);
            }
        } catch (...) {
            // BNLogError("failz xD");
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

    size_t addrSize = m_dscView->GetAddressSize();

    DefineTypedef(m_dscView, {CustomTypes::TaggedPointer}, Type::PointerType(addrSize, Type::VoidType()));
    DefineTypedef(m_dscView, {CustomTypes::FastPointer}, Type::PointerType(addrSize, Type::VoidType()));
    DefineTypedef(m_dscView, {CustomTypes::RelativePointer}, Type::IntegerType(4, true));

    DefineTypedef(m_dscView, {"id"}, Type::PointerType(addrSize, Type::VoidType()));
    DefineTypedef(m_dscView, {"SEL"}, Type::PointerType(addrSize, Type::IntegerType(1, false)));

    DefineTypedef(m_dscView, {"BOOL"}, Type::IntegerType(1, false));
    DefineTypedef(m_dscView, {"NSInteger"}, Type::IntegerType(addrSize, true));
    DefineTypedef(m_dscView, {"NSUInteger"}, Type::IntegerType(addrSize, false));
    DefineTypedef(m_dscView, {"CGFloat"}, Type::FloatType(addrSize));

    StructureBuilder cfstringStructBuilder;
    cfstringStructBuilder.AddMember(Type::PointerType(addrSize, Type::VoidType()), "isa");
    cfstringStructBuilder.AddMember(Type::IntegerType(addrSize, false), "flags");
    cfstringStructBuilder.AddMember(Type::PointerType(addrSize, Type::VoidType()), "data");
    cfstringStructBuilder.AddMember(Type::IntegerType(addrSize, false), "size");
    auto type = FinalizeStructureBuilder(m_dscView, cfstringStructBuilder, "CFString");
    m_types.CFString = type.first;

    StructureBuilder methodEntry;
    methodEntry.AddMember(Type::IntegerType(4, true), "name");
    methodEntry.AddMember(Type::IntegerType(4, true), "types");
    methodEntry.AddMember(Type::IntegerType(4, true), "imp");
    type = FinalizeStructureBuilder(m_dscView, methodEntry, "objc_method_entry_t");
    m_types.MethodListEntry = type.first;

    StructureBuilder method;
    method.AddMember(Type::PointerType(addrSize, Type::VoidType()), "name");
    method.AddMember(Type::PointerType(addrSize, Type::VoidType()), "types");
    method.AddMember(Type::PointerType(addrSize, Type::VoidType()), "imp");
    type = FinalizeStructureBuilder(m_dscView, method, "objc_method_t");
    m_types.Method = type.first;

    StructureBuilder methList;
    methList.AddMember(Type::IntegerType(4, false), "obsolete");
    methList.AddMember(Type::IntegerType(4, false), "count");
    type = FinalizeStructureBuilder(m_dscView, methList, "objc_method_list_t");
    m_types.MethodList = type.first;

    StructureBuilder classROBuilder;
    classROBuilder.AddMember(Type::IntegerType(4, false), "flags");
    classROBuilder.AddMember(Type::IntegerType(4, false), "start");
    classROBuilder.AddMember(Type::IntegerType(4, false), "size");
    if (addrSize == 8)
        classROBuilder.AddMember(Type::IntegerType(4, false), "reserved");
    classROBuilder.AddMember(Type::NamedType(m_dscView, CustomTypes::TaggedPointer), "ivar_layout");
    classROBuilder.AddMember(Type::NamedType(m_dscView, CustomTypes::TaggedPointer), "name");
    classROBuilder.AddMember(Type::NamedType(m_dscView, CustomTypes::TaggedPointer), "methods");
    classROBuilder.AddMember(Type::NamedType(m_dscView, CustomTypes::TaggedPointer), "protocols");
    classROBuilder.AddMember(Type::NamedType(m_dscView, CustomTypes::TaggedPointer), "ivars");
    classROBuilder.AddMember(Type::NamedType(m_dscView, CustomTypes::TaggedPointer), "weak_ivar_layout");
    classROBuilder.AddMember(Type::NamedType(m_dscView, CustomTypes::TaggedPointer), "properties");
    type = FinalizeStructureBuilder(m_dscView, classROBuilder, "objc_class_ro_t");
    m_types.ClassRO = type.first;

    StructureBuilder classBuilder;
    classBuilder.AddMember(Type::NamedType(m_dscView, CustomTypes::TaggedPointer), "isa");
    classBuilder.AddMember(Type::NamedType(m_dscView, CustomTypes::TaggedPointer), "super");
    classBuilder.AddMember(Type::NamedType(m_dscView, CustomTypes::TaggedPointer), "cache");
    classBuilder.AddMember(Type::NamedType(m_dscView, CustomTypes::TaggedPointer), "vtable");
    classBuilder.AddMember(Type::NamedType(m_dscView, CustomTypes::TaggedPointer), "data");
    type = FinalizeStructureBuilder(m_dscView, classBuilder, "objc_class_t");
    m_types.Class = type.first;

    StructureBuilder ivarBuilder;
    ivarBuilder.AddMember(Type::PointerType(addrSize, Type::IntegerType(4, false)), "offset");
    ivarBuilder.AddMember(Type::PointerType(addrSize, Type::VoidType()), "name");
    ivarBuilder.AddMember(Type::PointerType(addrSize, Type::VoidType()), "type");
    ivarBuilder.AddMember(Type::IntegerType(4, false), "alignment");
    ivarBuilder.AddMember(Type::IntegerType(4, false), "size");
    type = FinalizeStructureBuilder(m_dscView, ivarBuilder, "objc_ivar_t");
    // m_types. = type.first;

    StructureBuilder ivarList;
    ivarList.AddMember(Type::IntegerType(4, false), "entsize");
    ivarList.AddMember(Type::IntegerType(4, false), "count");
    type = FinalizeStructureBuilder(m_dscView, ivarList, "objc_ivar_list_t");

    m_typesLoaded = true;
}