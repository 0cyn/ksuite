//
// Created by kat on 5/9/23.
//

// PLEASE MAKE THESE PUBLIC AT SOME POINT THERES SO MUCH REALLY ANNOYING STUFF I DONT WANT TO REWRITE

#ifndef KSUITE_PRIV_H
#define KSUITE_PRIV_H
#pragma once

#include <QtCore/QAbstractItemModel>
#include <QtCore/QSortFilterProxyModel>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QStyledItemDelegate>
#include <QTimer>

#include "binaryninja-api/ui/filter.h"
#include "binaryninja-api/ui/sidebar.h"
#include "binaryninja-api/ui/uitypes.h"
#include "binaryninja-api/ui/tabwidget.h"
#include "binaryninja-api/ui/datatypelist.h"
#include "binaryninja-api/ui/viewlist.h"
#include "binaryninja-api/ui/addressindicator.h"
#include "binaryninja-api/ui/logview.h"
#include "binaryninja-api/ui/scriptingconsole.h"

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
