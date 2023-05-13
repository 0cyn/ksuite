//
// Created by vr1s on 5/7/23.
//

#include "DockableSidebar.h"
#include "binaryninjacore.h"
#include "ui/theme.h"
#include "ui/sidebar.h"
#include "priv.h"
#include "xreflist.h"
#include "typeview.h"
#include "variablelist.h"
#include "stackview.h"
#include "stringsview.h"
#include "taglist.h"
#include "minigraph.h"
#include "memorymap.h"

#include <QPainter>
#include <QStyleOptionButton>
#include <QDebug>
#include <QStylePainter>

double dist(QPointF p, QPointF q)
{
    auto dx = p.x() - q.x();
    auto dy = p.y() - q.y();
    auto dist = sqrt(abs(dx*dx) + abs(dy*dy));
    return dist;
}

bool isInvalidWidget(QWidget* widget)
{
    return (std::string(widget->metaObject()->className()) == "SidebarInvalidContextWidget");
}

DockableSidebar* ContextSidebarManager::SidebarForGlobalPos(QPointF pos)
{
    auto nearest = m_activeSidebars.at(0);
    double nearestDist = dist(pos, nearest->mapToGlobal(nearest->pos()));
    for (auto sidebar : m_activeSidebars)
    {
        auto d = dist(pos, sidebar->mapToGlobal(sidebar->pos()));
        if (d < nearestDist)
        {
            nearest = sidebar;
            nearestDist = d;
        }
    }
    return nearest;
}

void ContextSidebarManager::SetupSidebars()
{
    auto wholeMainViewSplitter = new QSplitter(nullptr);
    auto central = m_context->mainWindow()->centralWidget();

    auto leftSideFrame = new QFrame(central);
    QPalette pal = leftSideFrame->palette();
    pal.setColor(QPalette::Window, getThemeColor(SidebarBackgroundColor));
    leftSideFrame->setContentsMargins(0,0,0,0);

    m_leftContentView = new DockableSidebarContentView(wholeMainViewSplitter, central);
    m_leftContentView->m_sidebarCtx = this;
    m_leftContentView->m_left = true;
    m_leftContentView->setObjectName("leftContentView");
    m_leftContentView->setContentsMargins(0, 0, 0, 0);
    m_leftContentView->m_context = m_context;

    m_sidebarForPos[TopLeft] = new DockableSidebar(this, TopLeft, leftSideFrame);
    auto leftSidebarLayout = new QVBoxLayout(leftSideFrame);

    leftSidebarLayout->setContentsMargins(0,0,0,0);
    leftSidebarLayout->addWidget(m_sidebarForPos[TopLeft]);
    leftSidebarLayout->addStretch();
    m_sidebarForPos[BottomLeft] = new DockableSidebar(this, BottomLeft, leftSideFrame);
    leftSidebarLayout->addWidget(m_sidebarForPos[BottomLeft]);

    /* Right */

    m_rightContentView = new DockableSidebarContentView(wholeMainViewSplitter, central);
    m_rightContentView->m_sidebarCtx = this;
    m_rightContentView->m_context = m_context;

    auto rightSideFrame = new QFrame(central);
    rightSideFrame->setContentsMargins(0,0,0,0);

    m_sidebarForPos[TopRight] = new DockableSidebar(this, TopRight, rightSideFrame);
    auto rightSidebarLayout = new QVBoxLayout(rightSideFrame);

    rightSidebarLayout->setContentsMargins(0,0,0,0);
    rightSidebarLayout->addWidget(m_sidebarForPos[TopRight]);
    rightSidebarLayout->addStretch();
    m_sidebarForPos[BottomRight] = new DockableSidebar(this, BottomRight, rightSideFrame);
    rightSidebarLayout->addWidget(m_sidebarForPos[BottomRight]);

    wholeMainViewSplitter->addWidget(m_leftContentView);

    // sorry guys :3
    for (auto child : central->children())
    {
        if (std::string(child->metaObject()->className()) == "QSplitter")
        {
            auto mainView = qobject_cast<QSplitter*>(child);
            wholeMainViewSplitter->addWidget(mainView);
            break;
        }
    }
    auto frm = new QFrame(central);
    m_wholeLayout = new QHBoxLayout(frm);
    m_wholeLayout->setContentsMargins(0,0,0,0);
    central->layout()->addWidget(frm);
    m_wholeLayout->addWidget(leftSideFrame);
    wholeMainViewSplitter->setParent(central);
    wholeMainViewSplitter->addWidget(m_rightContentView);
    m_wholeLayout->addWidget(wholeMainViewSplitter);
    m_wholeLayout->addWidget(rightSideFrame);

    m_oldSidebar->setVisible(false);

    m_sidebarForPos[TopLeft]->m_containedTypes.push_back(new ComponentTreeSidebarWidgetType());
    m_sidebarForPos[TopLeft]->m_containedTypes.push_back(new TypeViewSidebarWidgetType());
    m_sidebarForPos[TopLeft]->m_containedTypes.push_back(new StringsViewSidebarWidgetType());

    m_sidebarForPos[TopRight]->m_containedTypes.push_back(new VariableListSidebarWidgetType());
    m_sidebarForPos[TopRight]->m_containedTypes.push_back(new StackViewSidebarWidgetType());
    m_sidebarForPos[TopRight]->m_containedTypes.push_back(new MemoryMapSidebarWidgetType());

    m_sidebarForPos[BottomLeft]->m_containedTypes.push_back(new CrossReferenceSidebarWidgetType());
    m_sidebarForPos[BottomLeft]->m_containedTypes.push_back(new MiniGraphSidebarWidgetType());

    m_sidebarForPos[BottomRight]->m_containedTypes.push_back(new TagListSidebarWidgetType());

    m_sidebarForPos[TopLeft]->m_contentView = m_leftContentView;
    m_sidebarForPos[BottomLeft]->m_contentView = m_leftContentView;
    m_sidebarForPos[TopRight]->m_contentView = m_rightContentView;
    m_sidebarForPos[BottomRight]->m_contentView = m_rightContentView;
    UpdateTypes();
}

void ContextSidebarManager::UpdateTypes()
{
    m_activeSidebars.clear();
    for (size_t i = 0; i < SidebarPosCount; i++)
    {
        if (auto it = m_sidebarForPos.find(SidebarPos(i)); it != m_sidebarForPos.end())
        {
            auto sidebar = it->second;
            auto replacement = new DockableSidebar(this, sidebar->m_sidebarPos, sidebar->parentWidget());
            replacement->ClearTypes();
            for (auto type : sidebar->m_containedTypes)
                replacement->AddType(type);
            replacement->UpdateForTypes();
            replacement->m_contentView = sidebar->m_contentView;
            m_sidebarForPos[sidebar->m_sidebarPos] = replacement;
            sidebar->parentWidget()->layout()->replaceWidget(sidebar, replacement);
            sidebar->deleteLater();
            m_activeSidebars.push_back(replacement);
        }
    }
}

OrientablePushButton::OrientablePushButton(DockableSidebar *parent)
        : QPushButton(parent)
{
    setStyleSheet(QString("QPushButton {border: 0px;} "
                          "QPushButton::hover {"
                          "background-color: ") + getThemeColor(SidebarBackgroundColor).darker(140).name()
                  + ";}");
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, getThemeColor(SidebarBackgroundColor));
    if (parent)
        setOrientation((parent->m_sidebarPos == TopLeft || parent->m_sidebarPos == BottomLeft ) ? OrientablePushButton::VerticalBottomToTop : OrientablePushButton::VerticalTopToBottom);
    else
        setOrientation(OrientablePushButton::VerticalBottomToTop); // placeholder w/ no content doesn't matter :D
    setPalette(pal);

    m_sidebar = parent;
}

OrientablePushButton::OrientablePushButton(const QString &text, DockableSidebar *parent)
        : QPushButton(text, parent)
{
    setStyleSheet(QString("QPushButton {border: 0px;} "
                          "QPushButton::hover {"
                          "background-color: ") + getThemeColor(SidebarBackgroundColor).darker(140).name()
                  + ";}");
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, getThemeColor(SidebarBackgroundColor));
    setOrientation((parent->m_sidebarPos == TopLeft || parent->m_sidebarPos == BottomLeft ) ? OrientablePushButton::VerticalBottomToTop : OrientablePushButton::VerticalTopToBottom);
    setPalette(pal);

    m_sidebar = parent;
}

OrientablePushButton::OrientablePushButton(const QString &text, QWidget *parent)
        : QPushButton(text, parent)
{
    setStyleSheet(QString("QPushButton {border: 0px;} "
                          "QPushButton::hover {"
                          "background-color: ") + getThemeColor(SidebarBackgroundColor).darker(140).name()
                  + ";}");
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, getThemeColor(SidebarBackgroundColor));
    setOrientation(OrientablePushButton::VerticalTopToBottom);
    setPalette(pal);
}

OrientablePushButton::OrientablePushButton(const QIcon &icon, const QString &text, DockableSidebar *parent)
        : QPushButton(icon, text, parent)
{
    setStyleSheet(QString("QPushButton {border: 0px; background-color: " + getThemeColor(SidebarBackgroundColor).name() +
                          ";} "
                          "QPushButton::hover {"
                          "background-color: ") + getThemeColor(SidebarBackgroundColor).darker(140).name()
                  + ";}");
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, getThemeColor(SidebarBackgroundColor));
    setOrientation((parent->m_sidebarPos == TopLeft || parent->m_sidebarPos == BottomLeft ) ? OrientablePushButton::VerticalBottomToTop : OrientablePushButton::VerticalTopToBottom);
    setPalette(pal);

    m_sidebar = parent;
}

QSize OrientablePushButton::sizeHint() const
{
    QSize sh = QPushButton::sizeHint();

    if (mOrientation != OrientablePushButton::Horizontal)
    {
        sh.transpose();
    }

    return sh;
}

void OrientablePushButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QStylePainter painter(this);
    QStyleOptionButton option;
    initStyleOption(&option);

    if (mOrientation == OrientablePushButton::VerticalTopToBottom)
    {
        painter.rotate(90);
        painter.translate(0, -1 * width());
        option.rect = option.rect.transposed();
    }

    else if (mOrientation == OrientablePushButton::VerticalBottomToTop)
    {
        painter.rotate(-90);
        painter.translate(-1 * height(), 0);
        option.rect = option.rect.transposed();
    }

    painter.drawControl(QStyle::CE_PushButton, option);
}

OrientablePushButton::Orientation OrientablePushButton::orientation() const
{
    return mOrientation;
}

void OrientablePushButton::setOrientation(const OrientablePushButton::Orientation &orientation)
{
    mOrientation = orientation;
}

void OrientablePushButton::mousePressEvent(QMouseEvent* event)
{
    if (m_sidebar)
    {
        if (event->button() == Qt::LeftButton) {
            m_maybeDragStarted = true;
            m_mousePressPos = event->globalPosition();
            m_mouseMovePos = event->globalPosition();
            m_nearest = m_context->SidebarForGlobalPos(m_mousePressPos);
        }
    }

    QPushButton::mousePressEvent(event);
}

void OrientablePushButton::mouseMoveEvent(QMouseEvent* event)
{
    if (m_sidebar)
    {
        if (event->buttons() == Qt::LeftButton) {
            if (m_maybeDragStarted)
            {
                m_beingDragged = true;
                m_context->DragStartedWithTarget(this);
                m_maybeDragStarted = false;
                raise();
                m_dragButton = new OrientablePushButton(text(), m_sidebar->parentWidget());
                m_dragButton->setOrientation(mOrientation);
                m_dragButton->setIcon(icon());
                m_sidebar->parentWidget()->parentWidget()->layout()->addWidget(m_dragButton);
                m_dragButton->setParent(m_sidebar->parentWidget()->parentWidget()->parentWidget());
                m_dragButton->setFixedWidth(QWidget::width());
                m_dragButton->setFixedHeight(QWidget::height());
                m_dragButton->setAutoFillBackground(true);
                m_sidebar->parentWidget()->parentWidget()->layout()->removeWidget(m_dragButton);
                m_dragButton->raise();
                m_dragButton->move(event->globalPosition().toPoint().x() - m_dragButton->mapToGlobal(m_dragButton->pos()).x(),
                                   event->globalPosition().toPoint().y() - m_dragButton->mapToGlobal(m_dragButton->pos()).y());
                m_dragButton->m_mousePressPos = event->globalPosition();
                m_dragButton->m_mouseMovePos = event->globalPosition();
            }

            auto curPos = mapToGlobal(pos());
            auto globalPos = event->globalPosition();
            auto diff = globalPos - m_mouseMovePos;
            auto newPos = mapFromGlobal(curPos + diff.toPoint());
            move(newPos.x(), newPos.y());
            {
                auto dcurPos = mapToGlobal(m_dragButton->pos());
                auto ddiff = globalPos - m_dragButton->m_mouseMovePos;
                auto dnewPos = m_dragButton->mapFromGlobal(dcurPos + ddiff.toPoint());
                m_dragButton->move(dnewPos.x(), dnewPos.y());
                m_dragButton->m_mouseMovePos = globalPos;
            }

            auto targetSidebar = m_context->SidebarForGlobalPos(globalPos);
            if (targetSidebar)
            {
                size_t targetIdx = targetSidebar->IdxForGlobalPos(mapToGlobal(pos()));
                targetSidebar->DisplayDropPlaceholderForHeldButton(this, targetIdx);
                if (targetSidebar != m_nearest)
                    m_nearest->RemovePlaceholder();
                m_nearest = targetSidebar;
            }

            m_mouseMovePos = globalPos;
        }
    }
    else
    {
        return;
    }

    QPushButton::mouseMoveEvent(event);
}

void OrientablePushButton::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_sidebar)
    {
        if (event->button() == Qt::LeftButton && m_beingDragged) {
            m_beingDragged = false;
            m_dragButton->setVisible(false);
            auto targetSidebar = m_context->SidebarForGlobalPos(m_mouseMovePos);
            if (targetSidebar)
            {
                size_t targetIdx = targetSidebar->IdxForGlobalPos(mapToGlobal(pos()));
                if (m_sidebar != targetSidebar || m_idx != targetIdx)
                {
                    m_sidebar->m_containedTypes.erase(
                            std::remove(m_sidebar->m_containedTypes.begin(), m_sidebar->m_containedTypes.end(), m_type),
                            m_sidebar->m_containedTypes.end());
                    targetSidebar->AddType(m_type, targetIdx);
                }
            }
            m_context->UpdateTypes();
        }
    }
    QPushButton::mouseReleaseEvent(event);
}

DockableSidebarContentView::DockableSidebarContentView(QSplitter* parentSplitter, QWidget* parent)
    : QWidget(parent)
{
    m_parentSplitter = parentSplitter;
    m_splitter = new QSplitter(Qt::Orientation::Vertical, this);
    m_splitter->setContentsMargins(0,0,0,0);
    setContentsMargins(0,0,0,0);
    QGridLayout *layout = new QGridLayout(this);
    SizeCheck();
    layout->addWidget(m_splitter, 0 , 0);
}

void DockableSidebarContentView::ActivateWidgetType(SidebarWidgetType* type, bool top, bool reset)
{
    if (!reset && top && m_topActive && m_topContents)
    {
        if (m_topContents->widget()->title().toStdString() == type->name().toStdString())
        {
            m_topContents->setParent(nullptr);
            m_topContents = nullptr;
            m_topType = nullptr;
            m_topActive = false;
            return;
        }
    }
    if (!reset && !top && m_botActive && m_bottomContents)
    {
        if (m_bottomContents->widget()->title().toStdString() == type->name().toStdString())
        {
            //m_bottomContents->deleteLater();
            m_bottomContents->setParent(nullptr);
            m_bottomContents = nullptr;
            m_bottomType = nullptr;
            m_botActive = false;
            return;
        }
    }
    if (top)
        m_topType = type;
    else
        m_bottomType = type;
    SidebarWidgetAndHeader* existingWidget = nullptr;
    auto frameIter = m_sidebarCtx->m_widgets.find(type->viewSensitive() ? m_sidebarCtx->m_context->getCurrentViewFrame() : nullptr);
    if (frameIter != m_sidebarCtx->m_widgets.end())
    {
        auto dataIter = frameIter->second.find(type->viewSensitive() ? m_sidebarCtx->m_context->getCurrentViewFrame()->getCurrentDataType() : QString());
        if (dataIter != frameIter->second.end())
        {
            auto widgetIter = dataIter->second.find(type);
            if (widgetIter != dataIter->second.end())
                existingWidget = widgetIter->second;
        }
    }
    if (existingWidget)
    {
        if (top)
            m_topContents = existingWidget;
        else
            m_bottomContents = existingWidget;
        SizeCheck();
        if (top)
        {
            if (m_topActive)
            {
                m_splitter->replaceWidget(0, existingWidget);
            }
            else
            {
                if (m_botActive)
                {
                    m_splitter->insertWidget(0, existingWidget);
                }
                else
                    m_splitter->addWidget(existingWidget);
            }
            m_topActive = true;
        }
        else
        {
            if (m_botActive)
            {
                if (m_topActive)
                {
                    m_splitter->replaceWidget(1, existingWidget);
                }
                else
                    m_splitter->addWidget(existingWidget);
            }
            else
            {
                m_splitter->addWidget(existingWidget);
            }
            m_botActive = true;
        }
    }
    else
    {
        SidebarWidget* widget;
        SidebarWidgetAndHeader* contents;
        if (type->viewSensitive())
        {
            if (m_context->getCurrentViewFrame())
            {
                widget = type->createWidget(m_context->getCurrentViewFrame(), m_context->getCurrentView()->getData());
                if (!widget)
                    widget = type->createInvalidContextWidget();
            }
            else
            {
                widget = type->createInvalidContextWidget();
            }
            contents = new SidebarWidgetAndHeader(widget, m_context->getCurrentViewFrame());
            m_sidebarCtx->m_widgets[m_sidebarCtx->m_context->getCurrentViewFrame()][m_sidebarCtx->m_context->getCurrentViewFrame()->getCurrentDataType()][type] = contents;

            // Send notifications for initial state
            if (m_context->getCurrentViewFrame())
            {
                widget->notifyViewChanged(m_context->getCurrentViewFrame());
                /*auto frameIter = m_currentViewLocation.find(m_context->getCurrentViewFrame());
                if (frameIter != m_currentViewLocation.end())
                {
                    auto typeIter = frameIter->second.find(m_dataType);
                    if (typeIter != frameIter->second.end())
                    {
                        widget->notifyViewLocationChanged(typeIter->second.first,
                                                          typeIter->second.second);
                        widget->notifyOffsetChanged(typeIter->second.second.getOffset());
                    }
                }*/
            }
        }
        else
        {
            widget = type->createWidget(nullptr, nullptr);
            if (!widget)
                widget = type->createInvalidContextWidget();
            contents = new SidebarWidgetAndHeader(widget, m_context->getCurrentViewFrame());
        }

        if (contents)
        {
            if (top)
                m_topContents = contents;
            else
                m_bottomContents = contents;
            SizeCheck();
            if (top)
            {
                if (m_topActive)
                {
                    m_splitter->replaceWidget(0, contents);
                }
                else
                {
                    if (m_botActive)
                    {
                        m_splitter->insertWidget(0, contents);
                    }
                    else
                        m_splitter->addWidget(contents);
                }
                m_topActive = true;
            }
            else
            {
                if (m_botActive)
                {
                    if (m_topActive)
                    {
                        m_splitter->replaceWidget(1, contents);
                    }
                    else
                        m_splitter->addWidget(contents);
                }
                else
                {
                    m_splitter->addWidget(contents);
                }
                m_botActive = true;
            }
        }
    }

    repaint();
}

void DockableSidebarContentView::SizeCheck()
{
    if (m_topActive || m_botActive) {
        setMaximumWidth(10000);
        setMinimumWidth(300);
        setMinimumWidth(0);
    }
    else {
        setMaximumWidth(0);
    }
}

DockableSidebar::DockableSidebar(ContextSidebarManager* context, SidebarPos pos, QWidget* parent)
{
    m_context = context;
    m_sidebarPos = pos;
    setParent(parent);
    setMinimumSize(20, 300);
    setMouseTracking(true);

    QPalette pal = palette();
    pal.setColor(QPalette::Window, getThemeColor(SidebarBackgroundColor));
    setPalette(pal);
    setAutoFillBackground(true);

    m_layout = new QVBoxLayout();

    m_layout->setContentsMargins(0,0,0,0);
    m_layout->addStretch();
    setLayout(m_layout);
    m_placeholderButton = nullptr;

    m_context->RegisterSidebar(this);
    raise();
}

void DockableSidebar::AddButton(OrientablePushButton* button)
{
    bool bottom = (m_sidebarPos == BottomLeft || m_sidebarPos == BottomRight);
    if (bottom)
        m_layout->addWidget(button);
    else
        m_layout->insertWidget(m_layout->count()-1, button);
    button->m_sidebar = this;
    m_buttons.push_back(button);
    button->m_context = m_context;
    if (parentWidget())
        parentWidget()->repaint();
    repaint();
}

void DockableSidebar::UpdateForTypes()
{
    for (auto button : m_buttons)
        layout()->removeWidget(button);
    if (m_placeholderButton)
        layout()->removeWidget(m_placeholderButton);
    delete layout();
    m_layout = new QVBoxLayout();

    m_layout->setContentsMargins(0,0,0,0);
    m_layout->addStretch();
    setLayout(m_layout);

    size_t i = 0;
    for (auto type : m_containedTypes)
    {
        auto button = new OrientablePushButton(type->name(), this);
        connect(button, &QPushButton::pressed, this, [this, type=type](){
            m_contentView->ActivateWidgetType(type, (m_sidebarPos == SidebarPos::TopLeft || m_sidebarPos == SidebarPos::TopRight));
        });
        button->setIcon(QIcon(QPixmap::fromImage(type->icon().inactive)));
        button->m_type = type;
        AddButton(button);
        button->m_idx = i++;
    }
    repaint();
    if (parentWidget())
        parentWidget()->repaint();
}

void DockableSidebar::ButtonMovingOut(OrientablePushButton* button)
{
    m_layout->removeWidget(button);
    m_buttons.erase(std::remove(m_buttons.begin(), m_buttons.end(), button), m_buttons.end());
}


size_t DockableSidebar::IdxForGlobalPos(QPoint pos)
{
    size_t idx = 0;
    //BNLogInfo("%d %d", pos.x(), pos.y());
    for (auto btn : m_buttons)
    {
        auto globPos = btn->mapToGlobal(btn->pos());
        //BNLogInfo("idx %d::> %d %d", idx, globPos.x(), globPos.y());
        if (globPos.y() > pos.y())
            return idx;
        idx++;
    }
    return idx;
}

void DockableSidebar::DisplayDropPlaceholderForHeldButton(OrientablePushButton* button, size_t idx)
{
    bool bottom = (m_sidebarPos == BottomLeft || m_sidebarPos == BottomRight);

    if (m_placeholderButton)
    {
        m_layout->removeWidget(m_placeholderButton);
    }
    m_placeholderButton = new QWidget();
    m_placeholderButton->setMouseTracking(false);
    m_placeholderButton->setFixedSize(button->size());
    m_placeholderButton->setStyleSheet(QString("QWidget {background-color: ") + getThemeColor(SidebarBackgroundColor).darker(40).name() + ";}");
    m_layout->insertWidget(bottom ? idx+1 : idx, m_placeholderButton);
}

void DockableSidebar::RemovePlaceholder()
{
    if (m_placeholderButton)
    {
        m_layout->removeWidget(m_placeholderButton);
    }
}


