//
// Created by Kat on 8/17/22.
//

#include "CallgraphGenerator.h"
#include "highlevelilinstruction.h"
#include "BS_thread_pool.hpp"


CallgraphGenerator* CallgraphGenerator::GetInstance(Ref<BinaryView> view)
{
    if (!callGraphInstances.count(view)) {
        callGraphInstances[view] = new CallgraphGenerator(view);
    }

    return callGraphInstances[view];
}

CallgraphGenerator::CallgraphGenerator(Ref<BinaryView> data) : m_data(data)
{
    m_data->RegisterNotification(this);
    m_logger = m_data->CreateLogger("Callgraph Generator");
    RebuildCache();
}


void CallgraphGenerator::RebuildCache()
{
    m_nodes.clear();
    m_nodeMap.clear();

    size_t fCount = m_data->GetAnalysisFunctionList().size();
    m_nodes.reserve(fCount);

    // Generate an initial set of nodes

    for (const auto& f : m_data->GetAnalysisFunctionList())
    {
        auto *node = new CallGraphNode();
        node->func = f;
        m_nodes.push_back(node);
        m_nodeMap[f] = node;
    }

    // Add edges to the nodes
    for (auto f : m_data->GetAnalysisFunctionList())
    {
        for (auto edge : m_data->GetCallers(f->GetStart()))
        {
            auto callerNode = m_nodeMap[edge.func];
            callerNode->edges.push_back(f);
            m_nodeMap[f]->incomingEdges.push_back(callerNode);
            callerNode->outgoingEdges.push_back(m_nodeMap[f]);
        }
    }

    m_cacheValid = true;
}

void CallgraphGenerator::OnAnalysisFunctionAdded(BinaryView*, Function*)
{
    m_cacheValid = false;
}

void CallgraphGenerator::OnAnalysisFunctionUpdated(BinaryView*, Function*)
{
    m_cacheValid = false;
}

void CallgraphGenerator::OnAnalysisFunctionRemoved(BinaryView*, Function*)
{
    m_cacheValid = false;
}

Ref<FlowGraph> CallgraphGenerator::GenerateCallgraph(CallGraphSettings settings)
{
    auto start = chrono::high_resolution_clock::now();

    auto func = m_currentTarget;

    m_logger->LogInfo("Generating Callgraph");

    if (!m_cacheValid)
    {
        RebuildCache();
    }

    Ref<FlowGraph> graph = new FlowGraph();

    auto baseNode = m_nodeMap[func];

    if (!settings.generateFull)
    {
        if (!baseNode)
            RebuildCache();
        if (!baseNode)
        {
            LogAlert("A serious issue occurred with flowgraph generation");

            auto node = new FlowGraphNode(graph);
            DisassemblyTextLine line;
            line.tokens.emplace_back(TextToken, "base node was null.");
            DisassemblyTextLine line2;
            line2.tokens.emplace_back(TextToken, "This shouldn't ever occur. Please file an issue");
            node->SetLines({line, line2});
            graph->AddNode(node);
            return graph;
        }
    }

    vector<CallGraphNode*> validNodes;

    if (!settings.generateFull)
    {
        validNodes.push_back(baseNode);

        vector<CallGraphNode*> upwardValidNodes;
        vector<CallGraphNode*> downwardValidNodes;

        vector<CallGraphNode*> iterNodes;
        size_t fullSize = m_nodeMap.size();

        iterNodes.reserve(fullSize);

        TrySetTaskTextAndProgress("Pruning set of valid nodes", 0, fullSize);
        size_t curProg = 0;
        size_t cycle = 0;
        int added = 0;
        int64_t depthLimit = settings.upwardScope != -1 ? settings.upwardScope : 500;
        int64_t depth = 0;
        if (settings.generateUpwards)
        {
            for (auto incoming: baseNode->incomingEdges)
            {
                upwardValidNodes.push_back(incoming);
                added += 1;
            }
            while (added)
            {
                depth += 1;
                if (depth == depthLimit)
                    break;

                iterNodes = vector(upwardValidNodes.end()-added, upwardValidNodes.end());
                cycle += 1;
                curProg += added;
                if (cycle == 10)
                {
                    TrySetTaskTextAndProgress("Pruning set of valid nodes", curProg, fullSize);
                    cycle = 0;
                }
                added = 0;
                for (auto node: iterNodes)
                {
                    if (node)
                    {
                        for (auto incoming : node->incomingEdges)
                        {
                            if (std::count(upwardValidNodes.begin(), upwardValidNodes.end(), incoming) == 0)
                            {
                                if (incoming)
                                {
                                    upwardValidNodes.push_back(incoming);
                                    added += 1;
                                }
                            }
                        }
                    }
                }

            }
        }

        added = 0;
        depth = 0;
        depthLimit = settings.downwardScope != -1 ? settings.downwardScope : 500;
        if (settings.generateDownwards)
        {
            for (auto incoming: baseNode->outgoingEdges)
            {
                if (incoming)
                {
                    downwardValidNodes.push_back(incoming);
                    added += 1;
                }
            }
            while (added)
            {
                depth += 1;
                if (depth == depthLimit)
                    break;

                iterNodes = vector(downwardValidNodes.end()-added, downwardValidNodes.end());

                cycle += 1;
                curProg += added;
                if (cycle == 10)
                {
                    TrySetTaskTextAndProgress("Pruning set of valid nodes", curProg, fullSize);
                    cycle = 0;
                }

                added = 0;
                for (auto node: iterNodes)
                {
                    if (node)
                    {
                        for (auto incoming: node->outgoingEdges)
                        {
                            if (std::count(downwardValidNodes.begin(), downwardValidNodes.end(), incoming) == 0)
                            {
                                if (incoming)
                                {
                                    downwardValidNodes.push_back(incoming);
                                    added += 1;
                                }
                            }
                        }
                    }
                }
            }
        }

        TrySetTaskTextAndProgress("Pruning set of valid nodes", fullSize, fullSize);

        if (!upwardValidNodes.empty())
            validNodes.insert( validNodes.end(), upwardValidNodes.begin(), upwardValidNodes.end() );
        if (!downwardValidNodes.empty())
            validNodes.insert( validNodes.end(), downwardValidNodes.begin(), downwardValidNodes.end() );
    }
    else
    {
        validNodes = m_nodes;
    }

    BS::thread_pool pool(std::thread::hardware_concurrency() - 1);
    vector<std::future<std::pair<CallGraphNode*, FlowGraphNode*>>> futures;
    m_done = 0;

    for (auto node : validNodes)
    {
        std::future<std::pair<CallGraphNode*, FlowGraphNode*>> nodeFuture = pool.submit([=, &done=m_done](){
            auto graphNode = new FlowGraphNode(graph);
            if (!node->func->m_object)
            {
                // Maybe this func has no hlil, most likely we are just hosed.
                return std::pair<CallGraphNode*, FlowGraphNode*>(node, graphNode);
            }

            vector<DisassemblyTextLine> lines;
            DisassemblyTextLine nline;

            auto tok = InstructionTextToken(CodeSymbolToken, node->func->GetSymbol()->GetFullName(), node->func->GetStart());
            tok.size = tok.size * 2;
            nline.tokens.push_back(tok);
            lines.push_back(nline);
            DisassemblyTextLine sline;
            lines.emplace_back(sline);
            if (settings.embedHLIL)
            {
                if (node->func && node->func->GetHighLevelIL())
                {
                    auto root = node->func->GetHighLevelIL()->GetRootExpr();
                    DisassemblySettings* settings = new DisassemblySettings();
                    settings->SetOption(IndentHLILBody, true);

                    if (node->func && node->func->GetHighLevelIL()) {
                        auto txt = node->func->GetHighLevelIL()->GetExprText(root, settings);
                        for (auto l : txt)
                        {
                            lines.push_back(l);
                        }
                    }
                }
            }
            graphNode->SetLines(lines);

            done++;
            return std::pair<CallGraphNode*, FlowGraphNode*>(node, graphNode);
        });
        futures.push_back(std::move(nodeFuture));
    }

    size_t last = 0;
    while (true)
    {
        if (m_done == futures.size())
            break;
        if (m_done > last + (futures.size()/10))
        {
            TrySetTaskTextAndProgress("Generating Nodes...", m_done, futures.size());
            last = m_done;
        }
    }

    pool.wait_for_tasks();
    TrySetTaskTextAndProgress("Generating Nodes...", futures.size(), futures.size());

    if (!m_data->m_object)
    {
        // Check if we're hosed.
        return graph;
    }
    for (auto &future : futures)
    {
        auto nodeAndGraphNode = future.get();
        auto node = nodeAndGraphNode.first;
        node->node = nodeAndGraphNode.second;
    }
    size_t i = 0;
    last = 0;
    size_t vnodeSize = validNodes.size();
    for (auto node : validNodes)
    {
        i++;
        if (i > last + (vnodeSize/10))
        {
            TrySetTaskTextAndProgress("Computing edges...", i, vnodeSize);
            last = i;
        }
        for (auto edge : node->edges) {
            if (m_nodeMap[edge]->node && std::count(validNodes.begin(), validNodes.end(), m_nodeMap[edge])) {
                node->node->AddOutgoingEdge(UnconditionalBranch, m_nodeMap[edge]->node);
            }
        }
        if (node->node)
            graph->AddNode(node->node);
    }

    auto stop = chrono::high_resolution_clock::now();

    auto duration = chrono::duration_cast<chrono::milliseconds>(stop - start);

    m_logger->LogInfo("Completed Callgraph Generation in %d ms", duration);

    return graph;
}

void CallgraphGenerator::TrySetTaskTextAndProgress(std::string text, size_t done, size_t jobs)
{
    m_logger->LogInfo("%s - [%d/%d]", text.c_str(), done, jobs);
}
