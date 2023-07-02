//
// Created by kat on 6/7/23.
//

#include <binaryninjaapi.h>
#include <uitypes.h>
#include "CallgraphGenerator.h"
using namespace BinaryNinja;

#include "Callgraph.h"
#include "uicontext.h"
#include "globalarea.h"

void showCallGraph(BinaryView* view, Function* func, const string& title, CallGraphSettings settings)
{
    auto* callgraph = CallgraphGenerator::GetInstance(view);
    callgraph->SetCurrentTarget(func);

    if (settings.generateFull && view->GetAnalysisFunctionList().size() > 100000)
    {
        LogAlert("Generating the visual layout for %lu functions may take an extremely long time to complete. "
                 "Please consider using scoped callgraphs instead. ", view->GetAnalysisFunctionList().size());
    }

    if (settings.generateFull && view->GetAnalysisFunctionList().size() > 4000)
    {
        if (ShowMessageBox("Confirm", "Generating the visual layout for "
                                      + to_string(view->GetAnalysisFunctionList().size()) + " items may take an extremely long time. "
                                      + "Please confirm you want to generate this graph.",
                           YesNoButtonSet, WarningIcon) != YesButton)
            return;
    }

    if (!settings.generateFull)
    {
        if (settings.generateDownwards)
        {
            string scopeString;
            if (GetTextLineInput(scopeString, "How many calls deep should we go? Leave empty to parse the entire tree",
                                 "Downward Callgraph Scope"))
            {
                uint64_t scope;
                std::string errors;
                if (BinaryView::ParseExpression(view, scopeString, scope, 0, errors))
                {
                    scope = min(scope, (uint64_t) 500);
                    settings.downwardScope = (int64_t) scope;
                }
            }
        }
        if (settings.generateUpwards)
        {
            string scopeString;
            if (GetTextLineInput(scopeString, "How many calls up should we go? Leave empty to parse the entire tree",
                                 "Upward Callgraph Scope"))
            {
                uint64_t scope;
                std::string errors;
                if (BinaryView::ParseExpression(view, scopeString, scope, 0, errors))
                {
                    scope = min(scope, (uint64_t) 500);
                    settings.upwardScope = (int64_t) scope;
                }
            }
        }
    }

    UIContext::activeContext()->globalArea()->focusWidget("Log");
    BackgroundThread::create()
            ->thenBackground([=](QVariant) {
                FlowGraph *graph = callgraph->GenerateCallgraph(settings);
                return QVariant::fromValue(static_cast<void*>(graph));
            })
            ->thenMainThread([=](QVariant var) {
                // Check if we still have a view object.
                // if the tab gets closed mid-analysis, the processing function also performs checks and will
                //      exit fairly quickly. If we *are* hosed, it will still return a proper partial graph, and
                //      this function will still be leaking a ref to the view. So we dont want to display that graph.
                if (callgraph->GetView()->m_object)
                {
                    auto graph = static_cast<FlowGraph*>(var.value<void*>());
                    callgraph->GetView()->ShowGraphReport(title, graph);
                }
            })->start();
}

void CallgraphToolInit() {

    // Dont do this.

    vector<CallGraphSettings> items;
    CallGraphSettings onlyNames;
    onlyNames.name = "Only Names";
    items.push_back(onlyNames);
    CallGraphSettings withCode;
    withCode.name = "With Psuedocode";
    withCode.embedHLIL = true;
    items.push_back(withCode);
    vector<string> alphaSortingForcers = {"\x01","\x02","\x03","\x04","\x05","\x06","\x07","\x08","\x09","\x10"};

    size_t i = 0;
    for (auto item : items)
    {
        i++;
        auto settings = item;
        settings.generateDownwards = true;
        PluginCommand::Register("Callgraph\\\x01 From Entry Point\\" + alphaSortingForcers[i] + settings.name,
                                "Generate a callgraph",
                                [settings](BinaryView* view)
                                {
                                    showCallGraph(view,
                                                  view->GetAnalysisEntryPoint(),
                                                  "Calls from "
                                                  + view->GetAnalysisEntryPoint()->GetSymbol()->GetFullName(),
                                                  settings);
                                },
                                [](BinaryView* view){return view->GetAnalysisEntryPoint().GetPtr();});


        PluginCommand::Register("Callgraph\\\x02————————————", "\x01", nullptr,
                                [](BinaryView* view){return false;});

        settings = item;
        settings.generateUpwards = true;
        PluginCommand::RegisterForFunction("Callgraph\\\x03 Upward\\" + alphaSortingForcers[i] + settings.name,
                                           "Generate a callgraph",
                                           [settings](BinaryView* view, Function* func)
                                           {
                                               showCallGraph(view, func,
                                                             "Calls up to " + func->GetSymbol()->GetFullName(),
                                                             settings);
                                           });

        settings = item;
        settings.generateDownwards = true;
        PluginCommand::RegisterForFunction("Callgraph\\\x04 Downward\\" + alphaSortingForcers[i] + settings.name,
                                           "Generate a callgraph",
                                           [settings](BinaryView* view, Function* func)
                                           {
                                               showCallGraph(view, func,
                                                             "Calls down from " + func->GetSymbol()->GetFullName(),
                                                             settings);
                                           });

        settings = item;
        settings.generateUpwards = true;
        settings.generateDownwards = true;
        PluginCommand::RegisterForFunction("Callgraph\\\x05 Bidirectional\\" + alphaSortingForcers[i] + settings.name,
                                           "Generate a callgraph",
                                           [settings](BinaryView* view, Function* func)
                                           {
                                               showCallGraph(view, func,
                                                             "Calls to and from " + func->GetSymbol()->GetFullName(),
                                                             settings);
                                           });

        PluginCommand::Register("Callgraph\\\x06————————————", "\x01", nullptr, [](BinaryView* view){return false;});

        settings = item;
        settings.generateFull = true;
        PluginCommand::Register("Callgraph\\\x07 Full Program Callgraph\\" + alphaSortingForcers[i] + settings.name,
                                "Generate a callgraph",
                                [settings](BinaryView* view)
                                {
                                    showCallGraph(view,
                                                  view->GetAnalysisEntryPoint(),
                                                  view->GetFile()->GetFilename() + " Callgraph",
                                                  settings);
                                });
    }
    return;
}