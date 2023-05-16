//
// Created by vr1s on 5/8/23.
//

#include "Notifications.h"
#include <QLayout>
#include <QApplication>
#include "ui/sidebar.h"
#include "DockableSidebar.h"

Notifications* Notifications::m_instance = nullptr;

QLayout* findParentLayout(QWidget* w, QLayout* topLevelLayout)
{
    for (QObject* qo: topLevelLayout->children())
    {
        QLayout* layout = qobject_cast<QLayout*>(qo);
        if (layout != nullptr)
        {
            if (layout->indexOf(w) > -1)
                return layout;
            else if (!layout->children().isEmpty())
            {
                layout = findParentLayout(w, layout);
                if (layout != nullptr)
                    return layout;
            }
        }
    }
    return nullptr;
}

QLayout* findParentLayout(QWidget* w)
{
    if (w->parentWidget() != nullptr)
        if (w->parentWidget()->layout() != nullptr)
            return findParentLayout(w, w->parentWidget()->layout());
    return nullptr;
}

void Notifications::init()
{
    m_instance = new Notifications;
    UIContext::registerNotification(m_instance);
}

void Notifications::OnContextOpen(UIContext* context)
{
    for (auto &widget : QApplication::allWidgets()) {
        if (widget && widget->metaObject() && std::string(widget->metaObject()->className()) == "Sidebar") {
            auto bar = static_cast<Sidebar*>(widget);
            //bar->setContainer(nullptr);
            auto layout = findParentLayout(widget);
            auto junk = new QWidget(context->mainWindow()->centralWidget());
            junk->setVisible(false);
            junk->setFixedSize(0, 0);
            bar->container()->setParent(junk);
            bar->container()->setVisible(false);
            m_ctxForSidebar[context] = new ContextSidebarManager(bar->container(), widget, layout, context);
            m_ctxForSidebar[context]->SetupSidebars();
            break;
        }
    }
}

void Notifications::OnContextClose(UIContext* context)
{
}

void Notifications::OnViewChange(UIContext *context, ViewFrame *frame, const QString &type)
{
    auto ctx = m_ctxForSidebar[context];
    ctx->ResetAllWidgets();
    ctx->UpdateTypes();
}

