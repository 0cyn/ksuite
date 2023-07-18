//
// Created by vr1s on 5/7/23.
//

#ifndef KSUITE_DOCKABLESIDEBAR_H
#define KSUITE_DOCKABLESIDEBAR_H


#include <QtWidgets>
#include "binaryninja-api/ui/uicontext.h"
#include "binaryninja-api/ui/sidebar.h"

class DockableSidebar;
class OrientablePushButton;
class ContextSidebarManager;
class DockableSidebarContentView;

enum SidebarPos {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    SidebarPosCount // KEEP AT END!
};

class ContextSidebarManager {
public:
    QSettings* m_settings;
private:

    std::vector<DockableSidebar*> m_activeSidebars;
    QHBoxLayout* m_wholeLayout;
    UIContext* m_context;
    QWidget* m_oldSidebar;
    SidebarWidgetContainer* m_oldContainer;
    QLayout* m_targetLayout;
    std::unordered_map<std::string, SidebarWidgetType*> m_registeredTypes;
    std::map<ViewFrame*, std::map<QString, std::map<SidebarWidgetType*, SidebarWidgetAndHeader*>>> m_widgets;
    std::map<ViewFrame*, std::map<QString, std::map<SidebarWidgetType*, SidebarWidgetAndHeader*>>> m_floatingWidgets;
    DockableSidebarContentView* m_leftContentView;
    DockableSidebarContentView* m_rightContentView;
    OrientablePushButton* m_currentDragTarget;
    std::unordered_map<SidebarPos, DockableSidebar*> m_sidebarForPos;
    std::map<SidebarWidgetType*, QWidget*> m_floatingWidgetContainers;
public:
    ContextSidebarManager(SidebarWidgetContainer* oldContainer, QWidget* oldSidebar, QLayout* oldLayout, UIContext* context)
            : m_oldContainer(oldContainer), m_oldSidebar(oldSidebar), m_targetLayout(oldLayout), m_context(context)
    {
        m_settings = new QSettings("ksuite");
    }

    SidebarWidgetType* TypeForName(std::string name)
    {
        if (const auto& it = m_registeredTypes.find(name); it != m_registeredTypes.end())
            return it->second;
        return nullptr;
    }

    std::vector<std::string> UnassignedWidgetTypes();
    void DeactivateType(SidebarWidgetType* type);
    DockableSidebar* SidebarForGlobalPos(QPointF);
    void SetupSidebars();
    void UpdateTypes();

    SidebarWidgetAndHeader* getWidgetForType(SidebarWidgetType* type);
    SidebarWidgetAndHeader* getExistingWidget(SidebarWidgetType* type);
    SidebarWidgetAndHeader* getExistingFloatingWidget(SidebarWidgetType* type);
    void deleteExistingWidget(SidebarWidgetType* type);
    void deleteExistingFloatingWidget(SidebarWidgetType* type);
    void setExistingWidget(SidebarWidgetType* type, SidebarWidgetAndHeader* contents);
    void setExistingFloatingWidget(SidebarWidgetType* type, SidebarWidgetAndHeader* contents);

    UIContext* uiContext() { return m_context; }
    void RegisterSidebar(DockableSidebar* sidebar)
    {
        m_activeSidebars.push_back(sidebar);
    }
    OrientablePushButton* dragTarget() { return m_currentDragTarget; };
    void DragStartedWithTarget(OrientablePushButton* target)
    {
        m_currentDragTarget = target;
    }
    void DragEnded()
    {
        m_currentDragTarget = nullptr;
    };
    void WidgetStartedFloating(SidebarWidgetType* type, SidebarWidgetAndHeader* widget, QPoint pos);
    bool IsWidgetFloating(SidebarWidgetType* type)
    {
        return m_floatingWidgetContainers.count(type) > 0;
    }
    void ActivateFloatingWidget(SidebarWidgetType* type)
    {
        if (!IsWidgetFloating(type))
            return;
        m_floatingWidgetContainers[type]->setWindowState(m_floatingWidgetContainers[type]->windowState() & ~Qt::WindowMinimized);
        //m_floatingWidgetContainers[type]->setAttribute(Qt::WA_ShowWithoutActivating);
        m_floatingWidgetContainers[type]->raise();
    }
    void ResetAllWidgets();
};


class OrientablePushButton : public QPushButton
{
    Q_OBJECT

public:
    enum Orientation {
        Horizontal,
        VerticalTopToBottom,
        VerticalBottomToTop
    };
private:

    friend DockableSidebar;

    ContextSidebarManager* m_context;
    DockableSidebar* m_sidebar;
    DockableSidebar* m_nearest;

    bool m_trashed = false;

    size_t m_idx;

    bool m_maybeDragStarted = false;
    bool m_beingDragged = false;
    size_t m_dragTripCount = 0;
    QPointF m_mousePressPos;
    QPointF m_mouseMovePos;
    OrientablePushButton* m_dragButton;
    SidebarWidgetType* m_type;
    Orientation m_Orientation = Horizontal;

public:
    OrientablePushButton(DockableSidebar * parent = nullptr);
    OrientablePushButton(const QString & text, DockableSidebar *parent = nullptr);
    OrientablePushButton(const QString & text, QWidget *parent = nullptr);
    OrientablePushButton(const QIcon & icon, const QString & text, DockableSidebar *parent = nullptr);

    SidebarWidgetType* getType() const { return m_type; }

    OrientablePushButton::Orientation orientation() const { return m_Orientation; };
    void setOrientation(const OrientablePushButton::Orientation &orientation);

    QSize sizeHint() const;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

    void paintEvent(QPaintEvent *event);
};

class DragAcceptingSplitter : public QSplitter
{
    ContextSidebarManager* m_context;
public:
    DragAcceptingSplitter(ContextSidebarManager* context, QWidget* parent);

    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
};


QString sidebarPosToString(SidebarPos pos);

class DockableSidebar : public QWidget {

    Q_OBJECT

    friend OrientablePushButton;
    friend ContextSidebarManager;

    DockableSidebarContentView* m_contentView;

    ContextSidebarManager* m_context;
    SidebarPos m_sidebarPos;

    QVBoxLayout* m_layout;
    QWidget* m_placeholderButton;
    std::vector<OrientablePushButton*> m_buttons;
public:

    DockableSidebar(ContextSidebarManager* context, SidebarPos pos,
                    DockableSidebarContentView* contentView, QWidget* parent);
    void AddButton(OrientablePushButton* button);
    void AddType(SidebarWidgetType* type, uint8_t idx = UINT8_MAX)
    {
        QStringList typeNames = m_context->m_settings->value("SidebarTypes-" + sidebarPosToString(m_sidebarPos)).toStringList();
        if (idx > typeNames.size())
            typeNames.push_back(type->name());
        else
            typeNames.insert(typeNames.begin()+idx, type->name());
        typeNames.removeDuplicates();
        m_context->m_settings->setValue("SidebarTypes-" + sidebarPosToString(m_sidebarPos), typeNames);
    }
    void RemoveType(SidebarWidgetType *type)
    {
        QStringList typeNames = m_context->m_settings->value("SidebarTypes-" + sidebarPosToString(m_sidebarPos)).toStringList();
        typeNames.removeAll(type->name());
        m_context->m_settings->setValue("SidebarTypes-" + sidebarPosToString(m_sidebarPos), typeNames);
    }
    std::vector<SidebarWidgetType*> GetTypes(bool includeUnassigned = true);
    void UpdateForTypes();

    void dropEvent(QDropEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void ButtonMovingOut(OrientablePushButton* button);
    size_t IdxForGlobalPos(QPoint);
    void DisplayDropPlaceholderForHeldButton(OrientablePushButton* button, size_t idx);
    void RemovePlaceholder();
    void HighlightActiveButton();
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

    bool m_left = false;
    QVBoxLayout* m_layout;
    SidebarWidgetType* m_topType;
    SidebarWidgetType* m_bottomType;
    SidebarWidgetAndHeader* m_topContents;
    SidebarWidgetAndHeader* m_bottomContents;
    QSplitter* m_parentSplitter;
    QSplitter* m_splitter;
    bool m_topActive = false;
    bool m_botActive = false;
    // DockableSidebar* m_linkedSidebar;
    UIContext* m_context;
    ContextSidebarManager* m_sidebarCtx;

public:
    DockableSidebarContentView(ContextSidebarManager* sidebarContext, UIContext* context, QSplitter* parentSplitter, bool left, QWidget* parent = nullptr);
    void SetTopWidget(SidebarWidgetAndHeader * widget);
    void SetBottomWidget(SidebarWidgetAndHeader * widget);
    bool topActive() const { return m_topActive; };
    bool bottomActive() const { return m_botActive; };
    SidebarWidgetType* topType() { return m_topType; };
    SidebarWidgetType* bottomType() { return m_bottomType; };

    void ActivateWidgetType(SidebarWidgetType* type, bool top, bool reset = false);
    void DeactivateWidgetType(SidebarWidgetType* type);
    QSize sizeHint() const override;
    void SizeCheck();
};


#endif //KSUITE_DOCKABLESIDEBAR_H
