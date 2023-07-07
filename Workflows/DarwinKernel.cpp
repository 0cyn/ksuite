//
// Created by vr1s on 12/17/22.
//

#include "DarwinKernel.h"
#include "lowlevelilinstruction.h"


typedef __int128 int128_t;
typedef unsigned __int128 uint128_t;


union vreg {
    uint128_t raw;
    uint64_t smaller[2];
    uint8_t fields[16];
};


void DarwinKernelWorkflow::FixBrokenSIMD(Ref<AnalysisContext> ctx)
{
    try {

        const auto func = ctx->GetFunction();
        const auto arch = func->GetArchitecture();
        const auto bv = func->GetView();

        const auto llil = ctx->GetLowLevelILFunction();
        if (!llil) {
            return;
        }
        const auto ssa = llil->GetSSAForm();
        if (!ssa) {
            return;
        }

        for (const auto& block : llil->GetBasicBlocks())
        {
            for ( size_t insnIndex = block->GetStart(), end = block->GetEnd(); insnIndex < end; ++insnIndex )
            {
                auto insn = llil->GetInstruction(insnIndex);
                if (insn.operation != LLIL_SET_REG)
                    continue;

                if ((insn.GetDestRegister() >= REG_V0_B0 && insn.GetDestRegister() <= REG_V29_B15) && ((insn.GetDestRegister() - REG_V0_B0) % 16) == 0)
                {
                    auto dat = bv->ReadBuffer(insn.address, 4);
                    std::vector<InstructionTextToken> result;
                    size_t s = 4;
                    bv->GetDefaultArchitecture()->GetInstructionText(static_cast<const uint8_t *>(dat.GetData()), insn.address, s, result);
                    if (result.at(0).text != "movi")
                        continue;

                    auto newDest = ((insn.GetDestRegister() - REG_V0_B0) / 16) + (REG_V0);
                    auto newConstEntry = llil->GetExpr(insn.operands[1]).operands[0];

                    vreg newConst;
                    for (unsigned char & field : newConst.fields)
                        field = newConstEntry;

                    auto newConstExpr = llil->Const(8, newConst.smaller[0], insn);
                    auto shiftAmount = llil->Const(8, 64, insn);
                    auto shiftLeftExpr = llil->ShiftLeft(16, newConstExpr, shiftAmount, 0, insn);
                    auto andExpr = llil->Or(16, newConstExpr, shiftLeftExpr, 0, insn);
                    insn.Replace(llil->SetRegister(16, newDest, andExpr, 0, insn));

                    for (int i = 1; i < 16; i++)
                    {
                        insn = llil->GetInstruction(insnIndex+i);
                        insn.Replace(llil->Nop());
                    }

                    llil->GenerateSSAForm();
                    llil->Finalize();
                }
            }
        }
    }
    catch (...)
    {

    }
}


void
DarwinKernelWorkflow::RewritePacInstructions (Ref<AnalysisContext> ctx) {
    try {
        const auto func = ctx->GetFunction() ;
        const auto arch = func->GetArchitecture() ;
        const auto bv = func->GetView() ;

        const auto llil = ctx->GetLowLevelILFunction() ;

        if ( ! llil ) { return ; }

        const auto ssa = llil->GetSSAForm() ;

        if ( ! ssa ) { return ; }

        for ( const auto & block : ssa->GetBasicBlocks() )
            for ( size_t insnIndex = block->GetStart(), end = block->GetEnd() ; insnIndex < end ; ++ insnIndex )
            {
                auto insn = ssa->GetInstruction(insnIndex) ;

                if (insn.operation == LLIL_JUMP)
                {
                    auto dest = insn.GetDestExpr();
                    if (dest.operation == LLIL_REG_SSA)
                    {
                        auto destReg = dest.GetSourceSSARegister();
                        if (destReg.reg >= arch->GetRegisterByName("x0") && destReg.reg <= arch->GetRegisterByName("x7"))
                        {
                            auto llilIndex = ssa->GetNonSSAInstructionIndex(insnIndex) ;
                            LowLevelILInstruction curInstr = llil->GetInstruction(llilIndex);
                            curInstr.Replace(llil->Call(llil->Register(8, destReg.reg)));

                            llil->GenerateSSAForm() ;
                            llil->Finalize() ;
                        }
                    }
                }

                if ( insn.operation == LLIL_INTRINSIC_SSA )
                {
                    auto llilIndex = ssa->GetNonSSAInstructionIndex(insnIndex) ;
                    LowLevelILInstruction curInstr ;

                    switch ( insn.GetIntrinsic() )
                    {
                        case ARM64_INTRIN_PACDA :
                        {
                            curInstr = llil->GetInstruction(llilIndex-2) ;

                            curInstr.Replace(llil->Nop(curInstr)) ;
                            curInstr = llil->GetInstruction(llilIndex-1) ;
                            curInstr.Replace(llil->Nop(curInstr)) ;
                            curInstr = llil->GetInstruction(llilIndex) ;
                            curInstr.Replace(llil->Nop(curInstr)) ;
                            break ;
                        }
                        case ARM64_INTRIN_AUTDA :
                            [&]()
                            {
                                curInstr = llil->GetInstruction(llilIndex-2) ;

                                curInstr.Replace(llil->Nop(curInstr)) ;
                                curInstr = llil->GetInstruction(llilIndex-1) ;
                                curInstr.Replace(llil->Nop(curInstr)) ;
                                curInstr = llil->GetInstruction(llilIndex) ;
                                curInstr.Replace(llil->Nop(curInstr));
                            }() ;
                            break ;
                        case ARM64_INTRIN_AUTIB :
                            [&]()
                            {
                                curInstr = llil->GetInstruction(llilIndex);
                                auto val = llil->Const(8, 0, curInstr);
                                curInstr.Replace(val);
                                curInstr = llil->GetInstruction(llilIndex+1);
                                curInstr.Replace(llil->SetRegister(8, x16, val));
                            }() ;
                            break ;
                        case ARM64_INTRIN_PACIA :
                        {
                            curInstr = llil->GetInstruction(llilIndex);
                            curInstr.Replace(llil->Nop(curInstr));
                            curInstr = llil->GetInstruction(llilIndex-1);
                            curInstr.Replace(llil->Nop(curInstr));
                            break ;
                        }
                        case ARM64_INTRIN_PACIB :
                        case ARM64_INTRIN_XPACD :
                        {
                            curInstr = llil->GetInstruction(llilIndex);
                            curInstr.Replace(llil->Nop(curInstr));
                            break ;
                        }
                        default :
                            continue ;
                    }

                    llil->GenerateSSAForm() ;
                    llil->Finalize() ;
                }
            }
    }
    catch (...) {

    }
}



void DarwinKernelWorkflow::DontJump(Ref<AnalysisContext> ctx)
{
    try {

        const auto func = ctx->GetFunction();
        const auto arch = func->GetArchitecture();
        const auto bv = func->GetView();

        const auto llil = ctx->GetLowLevelILFunction();
        if (!llil) {
            return;
        }
        const auto ssa = llil->GetSSAForm();
        if (!ssa) {
            return;
        }

        for (const auto& block : llil->GetBasicBlocks())
        {
            for ( size_t insnIndex = block->GetStart(), end = block->GetEnd(); insnIndex < end; ++insnIndex )
            {
                auto insn = llil->GetInstruction(insnIndex);
                if (insn.operation != LLIL_SET_REG)
                    continue;

                if ((insn.GetDestRegister() >= REG_V0_B0 && insn.GetDestRegister() <= REG_V29_B15) && ((insn.GetDestRegister() - REG_V0_B0) % 16) == 0)
                {
                    auto dat = bv->ReadBuffer(insn.address, 4);
                    std::vector<InstructionTextToken> result;
                    size_t s = 4;
                    bv->GetDefaultArchitecture()->GetInstructionText(static_cast<const uint8_t *>(dat.GetData()), insn.address, s, result);
                    if (result.at(0).text != "movi")
                        continue;

                    auto newDest = ((insn.GetDestRegister() - REG_V0_B0) / 16) + (REG_V0);
                    auto newConstEntry = llil->GetExpr(insn.operands[1]).operands[0];

                    vreg newConst;
                    for (unsigned char & field : newConst.fields)
                        field = newConstEntry;

                    auto newConstExpr = llil->Const(8, newConst.smaller[0], insn);
                    auto shiftAmount = llil->Const(8, 64, insn);
                    auto shiftLeftExpr = llil->ShiftLeft(16, newConstExpr, shiftAmount, 0, insn);
                    auto andExpr = llil->Or(16, newConstExpr, shiftLeftExpr, 0, insn);
                    insn.Replace(llil->SetRegister(16, newDest, andExpr, 0, insn));

                    for (int i = 1; i < 16; i++)
                    {
                        insn = llil->GetInstruction(insnIndex+i);
                        insn.Replace(llil->Nop());
                    }

                    llil->GenerateSSAForm();
                    llil->Finalize();
                }
            }
        }
    }
    catch (...)
    {

    }
}


static constexpr auto workflowInfo = R"({
  "title": "Darwin Kernel Tools",
  "description": "Darwin Kernel Tooling.",
  "capabilities": []
})" ;


void DarwinKernelWorkflow::Register()
{
    const auto wf = BinaryNinja::Workflow::Instance()->Clone("core.function.darwinKernel") ;
    wf->RegisterActivity(new BinaryNinja::Activity(
            "core.analysis.suap", &DarwinKernelWorkflow::RewritePacInstructions)) ;
    wf->RegisterActivity(new BinaryNinja::Activity(
            "core.analysis.suasimd", &DarwinKernelWorkflow::FixBrokenSIMD)) ;
    wf->Insert("core.function.translateTailCalls", "core.analysis.suap") ;
    wf->AssignSubactivities("core.analysis.suap", { "core.analysis.suasimd" }) ;
    //wf->Insert("core.analysis.suasimd", "core.analysis.suap");

    BinaryNinja::Workflow::RegisterWorkflow(wf, workflowInfo);
}
