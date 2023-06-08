//
// Created by kat on 6/7/23.
//
#include "Actions.h"
#include "MultiShortcut.h"
#include "../Callgraph/CallgraphGenerator.h"
#include "../Callgraph/Callgraph.h"

void RegisterActions(UIContext *context)
{
    UIAction::registerAction("KSuite/Callgraph");
    UIAction::registerAction("KSuite", QKeySequence(Qt::Key_K));

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

    context->globalActions()->bindAction("KSuite", UIAction([](const UIActionContext& ctx){
        auto ms = new MultiShortcut(ctx, ctx.widget);

        ms->setActionForItemIndex(0, new MultiShortcut::MultiShortcutItem(
                "KSuite/Callgraph",
                new QKeyCombination(Qt::Key_U),
                "Callgraph"
                ));

        auto cPos = ctx.widget->cursor().pos();
        ms->move(cPos);
        ms->show();
        ms->setFocus();
    }));
}