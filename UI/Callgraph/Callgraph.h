//
// Created by kat on 6/7/23.
//

#ifndef KSUITE_CALLGRAPH_H
#define KSUITE_CALLGRAPH_H

#include "CallgraphGenerator.h"

void showCallGraph(BinaryView* view, Function* func, const std::string& title, CallGraphSettings settings);
void CallgraphToolInit();

#endif //KSUITE_CALLGRAPH_H
