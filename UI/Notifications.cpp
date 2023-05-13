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
    BNLogInfo("%s", __FUNCTION__ );
    for (auto &widget : QApplication::allWidgets()) {
        if (std::string(widget->metaObject()->className()) == "Sidebar") {
            auto bar = static_cast<Sidebar*>(widget);
            //bar->setContainer(nullptr);
            auto layout = findParentLayout(widget);
            auto junk = new QWidget(context->mainWindow()->centralWidget());
            junk->setVisible(false);
            junk->setFixedSize(0, 0);
            bar->container()->setParent(junk);
            bar->container()->setVisible(false);
            m_ctxForSidebar[context] = new ContextSidebarManager();
            m_ctxForSidebar[context]->m_oldContainer = bar->container();
            m_ctxForSidebar[context]->m_oldSidebar = widget;
            m_ctxForSidebar[context]->m_targetLayout = layout;
            m_ctxForSidebar[context]->m_context = context;
            m_ctxForSidebar[context]->SetupSidebars();
        }
    }
}

void Notifications::OnContextClose(UIContext* context)
{
    BNLogInfo("%s", __FUNCTION__ );
}

void Notifications::OnViewChange(UIContext *context, ViewFrame *frame, const QString &type)
{
    BNLogInfo("%s", __FUNCTION__ );
    auto ctx = m_ctxForSidebar[context];
    ctx->m_oldContainer->setVisible(false);
    if (ctx->m_leftContentView)
    {
        if (ctx->m_leftContentView->m_topActive)
            ctx->m_leftContentView->ActivateWidgetType(ctx->m_leftContentView->m_topType, true, true);
        if (ctx->m_leftContentView->m_botActive)
            ctx->m_leftContentView->ActivateWidgetType(ctx->m_leftContentView->m_bottomType, false, true);
    }
    if (ctx->m_rightContentView)
    {
        if (ctx->m_rightContentView->m_topActive)
            ctx->m_rightContentView->ActivateWidgetType(ctx->m_rightContentView->m_topType, true, true);
        if (ctx->m_rightContentView->m_botActive)
            ctx->m_rightContentView->ActivateWidgetType(ctx->m_rightContentView->m_bottomType, false, true);
    }
    ctx->UpdateTypes();
}

