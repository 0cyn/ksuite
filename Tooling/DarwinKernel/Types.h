//
// Created by kat on 7/7/23.
//
#include <string>

#ifndef KSUITE_TYPES_H
#define KSUITE_TYPES_H

const std::string extArgs = "struct IOExternalMethodArguments\n"
                            "{\n"
                            "    uint32_t version;\n"
                            "    uint32_t selector;\n"
                            "    uint64_t asyncWakePort;\n"
                            "    uint64_t asyncReference;\n"
                            "    uint32_t asyncReferenceCount;\n"
                            "    uint64_t const* scalarInput;\n"
                            "    uint32_t scalarInputCount;\n"
                            "    void const* structureInput;\n"
                            "    uint32_t structureInputSize;\n"
                            "    uint64_t structureInputDescriptor;\n"
                            "    uint64_t* scalarOutput;\n"
                            "    uint32_t scalarOutputCount;\n"
                            "    void* structureOutput;\n"
                            "    uint32_t structureOutputSize;\n"
                            "    uint64_t structureOutputDescriptor;\n"
                            "    uint32_t structureOutputDescriptorSize;\n"
                            "    uint32_t __reservedA;\n"
                            "    uint64_t structureVariableOutputData;\n"
                            "    uint32_t __reserved[0x1e];\n"
                            "};";


#endif //KSUITE_TYPES_H
