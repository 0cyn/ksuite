//
// Created by kat on 5/9/23.
//

#ifndef KSUITE_PRIV_H
#define KSUITE_PRIV_H
#pragma once

#include <QtCore/QAbstractItemModel>
#include <QtCore/QSortFilterProxyModel>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QStyledItemDelegate>
#include <QTimer>

#include "filter.h"
#include "sidebar.h"
#include "uitypes.h"

#include <mutex>

using ComponentRef = BinaryNinja::Ref<BinaryNinja::Component>;

class ComponentFilterModel;
class ComponentTreeView;
class ComponentModel;
class ComponentTree;
class ComponentTreeAutoscrollArbiter;
struct ComponentModelQueuedUpdate;
struct UpdateState;

class BINARYNINJAUIAPI ComponentTreeSidebarWidgetType : public SidebarWidgetType
{
public:
    ComponentTreeSidebarWidgetType();
    SidebarWidget* createWidget(ViewFrame*, BinaryViewRef) override;
};

#endif //KSUITE_PRIV_H
