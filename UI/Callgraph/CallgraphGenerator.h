//
// Created by Kat on 8/17/22.
//

#ifndef YACP_CALLGRAPHGENERATOR_H
#define YACP_CALLGRAPHGENERATOR_H

#include "binaryninjaapi.h"
#include "binja/ui/progresstask.h"
#include <map>
#include <QString>
#include <utility>


using namespace std;
using namespace BinaryNinja;


struct CallGraphSettings {
    string name = string("");
    bool generateUpwards = false;
    bool generateDownwards = false;
    bool generateFull = false;
    bool embedHLIL = false;
    int64_t upwardScope = -1;
    int64_t downwardScope = -1;
};


struct CallGraphNode
{
    Ref<Function> func;
    vector<Ref<Function>> edges;

    vector<CallGraphNode*> incomingEdges;
    vector<CallGraphNode*> outgoingEdges;

    Ref<FlowGraphNode> node;
};

class CallgraphGenerator;

static map<Ref<BinaryView>, CallgraphGenerator*> callGraphInstances;
static map<CallgraphGenerator*, ProgressTask*> tasks;

class CallgraphGenerator : public BinaryDataNotification
{
    Ref<Function> m_currentTarget;

    Ref<BinaryView> m_data;
    Ref<Logger> m_logger;

    map<Ref<Function>, CallGraphNode*> m_nodeMap;
    vector<CallGraphNode*> m_nodes;

    std::atomic<int> m_done;

    bool m_cacheValid;

public:

    static CallgraphGenerator* GetInstance(Ref<BinaryView> view);

    CallgraphGenerator(Ref<BinaryView> data);

    bool ValidateAlive() { return m_data.GetPtr() != nullptr; };

    void SetCurrentTarget(Ref<Function> func) { m_currentTarget = std::move(func); };

    Ref<BinaryView> GetView() { return m_data; };

    void OnAnalysisFunctionAdded(BinaryView *view, Function *func) override;
    void OnAnalysisFunctionUpdated(BinaryView *view, Function *func) override;
    void OnAnalysisFunctionRemoved(BinaryView *view, Function *func) override;

    void RebuildCache();

    Ref<FlowGraph> GenerateCallgraph(CallGraphSettings settings);



    ProgressTask* CurrentTask() { return tasks.count(this) > 0 ? tasks[this] : nullptr; };

    void TrySetTaskTextAndProgress(std::string text, size_t done, size_t jobs);

};


#endif //YACP_CALLGRAPHGENERATOR_H
