//
// Created by vr1s on 5/7/23.
//

#ifndef KSUITE_DOCKABLESIDEBAR_H
#define KSUITE_DOCKABLESIDEBAR_H


#include <QtWidgets>
#include "uicontext.h"
#include "sidebar.h"

class DockableSidebar;
class OrientablePushButton;
class ContextSidebarManager;
class DockableSidebarContentView;

class OrientablePushButton : public QPushButton
{
Q_OBJECT

    friend DockableSidebar;

    ContextSidebarManager* m_context;
    DockableSidebar* m_sidebar;
    DockableSidebar* m_nearest;

public:
    enum Orientation {
        Horizontal,
        VerticalTopToBottom,
        VerticalBottomToTop
    };

    bool m_maybeDragStarted = false;
    bool m_beingDragged = false;
    QPointF m_mousePressPos;
    QPointF m_mouseMovePos;
    OrientablePushButton* m_dragButton;
    SidebarWidgetType* m_type;

    OrientablePushButton(DockableSidebar * parent = nullptr);
    OrientablePushButton(const QString & text, DockableSidebar *parent = nullptr);
    OrientablePushButton(const QString & text, QWidget *parent = nullptr);
    OrientablePushButton(const QIcon & icon, const QString & text, DockableSidebar *parent = nullptr);

    QSize sizeHint() const;

    OrientablePushButton::Orientation orientation() const;
    void setOrientation(const OrientablePushButton::Orientation &orientation);
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

protected:
    void paintEvent(QPaintEvent *event);

private:
    Orientation mOrientation = Horizontal;
};

enum SidebarPos {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    SidebarPosCount // KEEP AT END!
};

class DockableSidebar : public QWidget {

    Q_OBJECT

    friend OrientablePushButton;
    friend ContextSidebarManager;

    std::vector<SidebarWidgetType*> m_containedTypes;
    DockableSidebarContentView* m_contentView;

    ContextSidebarManager* m_context;
    SidebarPos m_sidebarPos;

    QVBoxLayout* m_layout;
    QWidget* m_placeholderButton;
    std::vector<OrientablePushButton*> m_buttons;
public:

    DockableSidebar(ContextSidebarManager* context, SidebarPos pos, QWidget* parent);
    void AddButton(OrientablePushButton* button);
    void AddType(SidebarWidgetType* type)
    {
        m_containedTypes.push_back(type);
    }
    void ClearTypes()
    {
        for (auto button : m_buttons)
            m_layout->removeWidget(button);
        m_containedTypes.clear();
    }
    void UpdateForTypes();

    void ButtonMovingOut(OrientablePushButton* button);
    size_t IdxForGlobalPos(QPoint);
    void DisplayDropPlaceholderForHeldButton(OrientablePushButton* button, size_t idx);
    void RemovePlaceholder();
};


class ExpandingSplitter : public QSplitter {

    QWidget* m_parent;

    Q_OBJECT
public:
    ExpandingSplitter(Qt::Orientation orientation = Qt::Vertical, QWidget* parent = nullptr)
        : QSplitter(orientation, parent), m_parent(parent){
    }
};


class DockableSidebarContentView : public QWidget {

    Q_OBJECT

public:
    bool m_left = false;
    QVBoxLayout* m_layout;
    QSplitter* m_parentSplitter;
    QSplitter* m_splitter;
    bool m_topActive = false;
    bool m_botActive = false;
    // DockableSidebar* m_linkedSidebar;
    UIContext* m_context;
    DockableSidebarContentView(QSplitter* parentSplitter, QWidget* parent = nullptr);

    void ActivateWidgetType(SidebarWidgetType* type, bool top);
};


class ContextSidebarManager {
public:
    std::vector<DockableSidebar*> m_activeSidebars;
    QHBoxLayout* m_wholeLayout;
    UIContext* m_context;
    QWidget* m_oldSidebar;
    QLayout* m_targetLayout;
    OrientablePushButton* m_currentDragTarget;
    std::map<SidebarWidgetType*, DockableSidebar*> m_sidebarForWidgetType;
    std::unordered_map<SidebarPos, DockableSidebar*> m_sidebarForPos;
    DockableSidebar* SidebarForGlobalPos(QPointF);
    void SetupSidebars();
    void UpdateTypes();
    void RegisterSidebar(DockableSidebar* sidebar)
    {
        m_activeSidebars.push_back(sidebar);
    }
    void DragStartedWithTarget(OrientablePushButton* target)
    {
        m_currentDragTarget = target;
    }
    void DragEnded()
    {
        m_currentDragTarget = nullptr;
    };
};


#endif //KSUITE_DOCKABLESIDEBAR_H
