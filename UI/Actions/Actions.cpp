//
// Created by kat on 6/7/23.
//
#include "Actions.h"
#include "MultiShortcut.h"
#include "../Callgraph/CallgraphGenerator.h"
#include "../Callgraph/Callgraph.h"
#include "XNU/UI/TypeSetter.h"
#include "XNU/CPP/CPPTypeHelper.h"

void RegisterActions(UIContext *context)
{
    UIAction::registerAction("KSuite/Callgraph");
    UIAction::registerAction("KSuite", QKeySequence(Qt::Key_K));
    UIAction::registerAction("KSuite/Callgraph/Upward/Names");
    UIAction::registerAction("KSuite/Callgraph/Upward/With Code");
    UIAction::registerAction("KSuite/Callgraph/Downward/Names");
    UIAction::registerAction("KSuite/Callgraph/Downward/With Code");
    UIAction::registerAction("KSuite/Callgraph/Bidirectional/Names");
    UIAction::registerAction("KSuite/Callgraph/Bidirectional/With Code");
    UIAction::registerAction("KSuite/Callgraph/Upward");
    UIAction::registerAction("KSuite/Callgraph/Downward");
    UIAction::registerAction("KSuite/Callgraph/Bidirectional");
    UIAction::registerAction("KSuite/Callgraph");
    UIAction::registerAction("KSuite/KernelTypes/ExtMethod");
    UIAction::registerAction("KSuite/KernelTypes/ThisArg");
    UIAction::registerAction("KSuite/KernelTypes/RunAnalysis");
    UIAction::registerAction("KSuite/KernelTypes");

    context->globalActions()->bindAction("KSuite/Callgraph/Upward/Names", UIAction([](const UIActionContext& ctx){
        CallGraphSettings settings;
        settings.name = "KSuite/Callgraph/Upward/Names";
        settings.generateUpwards = true;
        settings.embedHLIL = false;
        showCallGraph(ctx.binaryView, ctx.function, "Calls up to " + ctx.function->GetSymbol()->GetFullName(), settings);
    }));
    context->globalActions()->bindAction("KSuite/Callgraph/Upward/With Code", UIAction([](const UIActionContext& ctx){
        CallGraphSettings settings;
        settings.name = "KSuite/Callgraph/Upward/With Code";
        settings.generateUpwards = true;
        settings.embedHLIL = true;
        showCallGraph(ctx.binaryView, ctx.function, "Calls up to " + ctx.function->GetSymbol()->GetFullName(), settings);
    }));
    context->globalActions()->bindAction("KSuite/Callgraph/Downward/Names", UIAction([](const UIActionContext& ctx){
        CallGraphSettings settings;
        settings.name = "KSuite/Callgraph/Downward/Names";
        settings.generateDownwards = true;
        settings.embedHLIL = false;
        showCallGraph(ctx.binaryView, ctx.function, "Calls from" + ctx.function->GetSymbol()->GetFullName(), settings);
    }));
    context->globalActions()->bindAction("KSuite/Callgraph/Downward/With Code", UIAction([](const UIActionContext& ctx){
        CallGraphSettings settings;
        settings.name = "KSuite/Callgraph/Downward/With Code";
        settings.generateDownwards = true;
        settings.embedHLIL = true;
        showCallGraph(ctx.binaryView, ctx.function, "Calls from " + ctx.function->GetSymbol()->GetFullName(), settings);
    }));

    context->globalActions()->bindAction("KSuite/Callgraph/Bidirectional/Names", UIAction([](const UIActionContext& ctx){
        CallGraphSettings settings;
        settings.name = "KSuite/Callgraph/Bidirectional/Names";
        settings.generateDownwards = true;
        settings.generateUpwards = true;
        settings.embedHLIL = false;
        showCallGraph(ctx.binaryView, ctx.function, "Calls to and from" + ctx.function->GetSymbol()->GetFullName(), settings);
    }));
    context->globalActions()->bindAction("KSuite/Callgraph/Bidirectional/With Code", UIAction([](const UIActionContext& ctx){
        CallGraphSettings settings;
        settings.name = "KSuite/Callgraph/Bidirectional/With Code";
        settings.generateDownwards = true;
        settings.generateUpwards = true;
        settings.embedHLIL = true;
        showCallGraph(ctx.binaryView, ctx.function, "Calls to and from " + ctx.function->GetSymbol()->GetFullName(), settings);
    }));

    context->globalActions()->bindAction("KSuite/Callgraph/Upward", UIAction([](const UIActionContext& ctx){
        auto ms = new MultiShortcut(ctx, ctx.widget);

        ms->setActionForItemIndex(0, new MultiShortcut::MultiShortcutItem(
                "KSuite/Callgraph/Upward/Names",
                new QKeyCombination(Qt::Key_U),
                "Names Only"
        ));
        ms->setActionForItemIndex(1, new MultiShortcut::MultiShortcutItem(
                "KSuite/Callgraph/Upward/With Code",
                new QKeyCombination(Qt::Key_I),
                "With Code"
        ));

        auto cPos = ctx.widget->cursor().pos();
        ms->move(cPos);
        ms->show();
    }));
    context->globalActions()->bindAction("KSuite/Callgraph/Downward", UIAction([](const UIActionContext& ctx){
        auto ms = new MultiShortcut(ctx, ctx.widget);

        ms->setActionForItemIndex(0, new MultiShortcut::MultiShortcutItem(
                "KSuite/Callgraph/Downward/Names",
                new QKeyCombination(Qt::Key_U),
                "Names Only"
        ));
        ms->setActionForItemIndex(1, new MultiShortcut::MultiShortcutItem(
                "KSuite/Callgraph/Downward/With Code",
                new QKeyCombination(Qt::Key_I),
                "With Code"
        ));

        auto cPos = ctx.widget->cursor().pos();
        ms->move(cPos);
        ms->show();
    }));
    context->globalActions()->bindAction("KSuite/Callgraph/Bidirectional", UIAction([](const UIActionContext& ctx){
        auto ms = new MultiShortcut(ctx, ctx.widget);

        ms->setActionForItemIndex(0, new MultiShortcut::MultiShortcutItem(
                "KSuite/Callgraph/Bidirectional/Names",
                new QKeyCombination(Qt::Key_U),
                "Names Only"
        ));
        ms->setActionForItemIndex(1, new MultiShortcut::MultiShortcutItem(
                "KSuite/Callgraph/Bidirectional/With Code",
                new QKeyCombination(Qt::Key_I),
                "With Code"
        ));

        auto cPos = ctx.widget->cursor().pos();
        ms->move(cPos);
        ms->show();
    }));

    context->globalActions()->bindAction("KSuite/Callgraph", UIAction([](const UIActionContext& ctx){
        auto ms = new MultiShortcut(ctx, ctx.widget);

        ms->setActionForItemIndex(0, new MultiShortcut::MultiShortcutItem(
                "KSuite/Callgraph/Upward",
                new QKeyCombination(Qt::Key_U),
                "Upward"
        ));
        ms->setActionForItemIndex(1, new MultiShortcut::MultiShortcutItem(
                "KSuite/Callgraph/Downward",
                new QKeyCombination(Qt::Key_I),
                "Downward"
        ));
        ms->setActionForItemIndex(2, new MultiShortcut::MultiShortcutItem(
                "KSuite/Callgraph/Bidirectional",
                new QKeyCombination(Qt::Key_O),
                "Bidirectional"
        ));

        auto cPos = ctx.widget->cursor().pos();
        ms->move(cPos);
        ms->show();
    }));

    context->globalActions()->bindAction("KSuite/KernelTypes/ExtMethod", UIAction([](const UIActionContext& ctx){
        auto helper = new CPPTypeHelper(ctx.binaryView);
        if (auto type = helper->GetClassTypeForFunction(ctx.function))
        {
            helper->SetExternalMethodType(ctx.function, type);
        }
    }));
    context->globalActions()->bindAction("KSuite/KernelTypes/ThisArg", UIAction([](const UIActionContext& ctx){
        auto helper = new CPPTypeHelper(ctx.binaryView);
        if (auto type = helper->GetClassTypeForFunction(ctx.function))
        {
            helper->SetThisArgType(ctx.function, type);
        }
    }));
    context->globalActions()->bindAction("KSuite/KernelTypes/RunAnalysis", UIAction([](const UIActionContext& ctx){
        if (ctx.binaryView)
        {
            auto helper = new CPPTypeHelper(ctx.binaryView);
            auto classes = helper->FetchClasses();
            for (auto& c : classes)
            {
                /*
                BNLogInfo("Class: %s", c.name.c_str());
                for (auto& s : c.superclasses)
                    BNLogInfo("  Superclass: %s", s.c_str());
                for (auto& it : c.vtable)
                {
                    std::string name;
                    if (auto sym = ctx.binaryView->GetSymbolByAddress(it.second))
                        name = sym->GetShortName();
                    if (name.empty())
                        if (auto sym = ctx.binaryView->GetSymbolByAddress(c.vtableStart + 0x10 + it.first))
                            name = sym->GetShortName();
                    BNLogInfo("  0x%llx = %s", it.first, name.c_str());
                }
                 */
                helper->CreateTypeForClass(c);
            }
        }
    }));


    context->globalActions()->bindAction("KSuite/KernelTypes", UIAction([](const UIActionContext& ctx){
        auto ms = new MultiShortcut(ctx, ctx.widget);

        ms->setActionForItemIndex(0, new MultiShortcut::MultiShortcutItem(
                "KSuite/KernelTypes/ExtMethod",
                new QKeyCombination(Qt::Key_U),
                "Ext Method"
        ));
        ms->setActionForItemIndex(1, new MultiShortcut::MultiShortcutItem(
                "KSuite/KernelTypes/ThisArg",
                new QKeyCombination(Qt::Key_I),
                "Set `this`"
        ));
        ms->setActionForItemIndex(2, new MultiShortcut::MultiShortcutItem(
                "KSuite/KernelTypes/RunAnalysis",
                new QKeyCombination(Qt::Key_O),
                "Analyze"
        ));

        auto cPos = ctx.widget->cursor().pos();
        ms->move(cPos);
        ms->show();
    }));

    context->globalActions()->bindAction("KSuite", UIAction([](const UIActionContext& ctx){
        auto ms = new MultiShortcut(ctx, ctx.widget);

        ms->setActionForItemIndex(0, new MultiShortcut::MultiShortcutItem(
                "KSuite/Callgraph",
                new QKeyCombination(Qt::Key_U),
                "Callgraph"
        ));
        ms->setActionForItemIndex(1, new MultiShortcut::MultiShortcutItem(
                "KSuite/KernelTypes",
                new QKeyCombination(Qt::Key_I),
                "XNU Tools"
        ));


        auto cPos = ctx.widget->cursor().pos();
        ms->move(cPos);
        ms->show();
        ms->setFocus();
    }));
}