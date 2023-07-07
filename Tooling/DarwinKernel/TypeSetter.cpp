//
// Created by kat on 7/7/23.
//

#include "TypeSetter.h"

std::vector<std::string> split(const std::string& s, char seperator)
{
    std::vector<std::string> output;

    std::string::size_type prev_pos = 0, pos = 0;

    while((pos = s.find(seperator, pos)) != std::string::npos)
    {
        std::string substring( s.substr(prev_pos, pos-prev_pos) );

        output.push_back(substring);

        prev_pos = ++pos;
    }

    output.push_back(s.substr(prev_pos, pos-prev_pos)); // Last word

    return output;
}

bool TypeSetter::CreateForContext(UIActionContext ctx)
{
    auto data = ctx.binaryView;
    if (!data)
        return false;
    auto func = ctx.function;
    if (!func)
        return false;

    auto storedData = new TypeSetterViewMetadata;
    if (auto metadata = data->QueryMetadata(TypeSetterViewMetadataKey))
        storedData->LoadFromMetadata(metadata);

    Ref<Symbol> sym = func->GetSymbol();
    std::string className;
    if (sym)
    {
        auto parts = split(sym->GetShortName(), ':');
        if (parts.size() > 1 && parts[0].size() > 1)
            className = parts[0];
    }
    if (className.empty())
    {
        if (!BinaryNinja::GetTextLineInput(className, "Class Name", "Class Name"))
            return false;
    }

    auto undoID = data->BeginUndoActions();

    Ref<Type> classType;

    if (auto it = std::find(storedData->classes.begin(), storedData->classes.end(), className); it != storedData->classes.end())
    {
        if (auto it2 = storedData->classQualNames.find(*it); it2 != storedData->classQualNames.end())
        {
            classType = data->GetTypeByName(it2->second);
        }
    }

    // i feel like this has to be a bit more complex than it should...
    if (!classType)
    {
        auto cTBuilder = StructureBuilder();
        cTBuilder.AddMember(Type::PointerType(8, Type::VoidType()), "vtable");
        auto stru = cTBuilder.Finalize();
        QualifiedName struName = QualifiedName(className);
        Ref<Type> struType = Type::StructureType(stru);
        auto assignedName = data->DefineType(Type::GenerateAutoTypeId("ksuite", className), struName, struType);
        classType = data->GetTypeByName(assignedName);
        storedData->classes.push_back(className);
        storedData->classQualNames[className] = assignedName.GetString();
    }

    Ref<Type> ioReturnType = data->GetTypeByName(QualifiedName("IOReturn"));
    if (!ioReturnType)
    {
        auto krtName = data->DefineType(Type::GenerateAutoTypeId("ksuite", QualifiedName("kern_return_t")), QualifiedName("kern_return_t"), Type::IntegerType(4, false));
        auto iorName = data->DefineType(Type::GenerateAutoTypeId("ksuite", QualifiedName("IOReturn")), QualifiedName("IOReturn"), data->GetTypeByName(krtName));
        ioReturnType = data->GetTypeByName(iorName);
    }

    Ref<Type> argsType = data->GetTypeByName(QualifiedName("IOExternalMethodArguments"));
    if (!argsType)
    {
        std::string errors;
        TypeParserResult tpResult;
        auto ok = data->ParseTypesFromSource(extArgs, {}, {}, tpResult, errors);
        if (ok && !tpResult.types.empty())
        {
            data->DefineType(Type::GenerateAutoTypeId("ksuite", QualifiedName("IOExternalMethodArguments")), QualifiedName("IOExternalMethodArguments"), tpResult.types[0].type);
        }
        argsType = data->GetTypeByName(QualifiedName("IOExternalMethodArguments"));
    }

    auto funcTypeBuilder = TypeBuilder::FunctionType(ioReturnType, func->GetCallingConvention(),
                                                     {FunctionParameter("target", Type::PointerType(8, classType)),
                                                      FunctionParameter("reference", Type::PointerType(8, Type::VoidType())),
                                                      FunctionParameter("args", Type::PointerType(8, argsType))});

    auto funcType = funcTypeBuilder.Finalize();

    if (funcType)
    {
        func->SetUserType(funcType);
    }

    data->StoreMetadata(TypeSetterViewMetadataKey, storedData->AsMetadata());

    func->Reanalyze();

    data->CommitUndoActions(undoID);

    return true;
}
