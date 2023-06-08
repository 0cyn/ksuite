//
// Created by serket on 5/24/23.
//

#ifndef KSUITE_MULTISHORTCUT_H
#define KSUITE_MULTISHORTCUT_H


#include <QWidget>
#include "action.h"
#include "uicontext.h"

class MultiShortcut : public QWidget {

    Q_OBJECT

    UIActionContext m_ctx;

    std::vector<QWidget*> m_wheelItems;
public:
    MultiShortcut(UIActionContext ctx, QWidget* parent);
    struct MultiShortcutItem {
        std::string action;
        QKeyCombination* keybind;
        QString text;
        MultiShortcutItem(std::string action, QKeyCombination* keybind, QString text)
            : action(action), keybind(keybind), text(text)
        {}
    public:
        bool valid = true;
    };
private:
    std::vector<MultiShortcutItem*> m_actions;
public:
    void setActionForItemIndex(size_t idx, MultiShortcutItem* item);
    void executeItemForIndex(size_t idx)
    {
        m_ctx.context->getCurrentActionHandler()->executeAction(QString::fromStdString(m_actions[idx]->action));
    }
    void keyPressEvent(QKeyEvent* event) override
    {
        BNLogInfo("%s", event->text().toStdString().c_str());
        event->accept();
        size_t i = 0;
        for (auto action : m_actions)
        {
            if (!action)
                continue;
            if (event->keyCombination() == *action->keybind)
            {
                close();
                releaseKeyboard();
                m_ctx.context->mainWindow()->setFocus();
                if (action->valid)
                    executeItemForIndex(i);
                return;
            }
            i++;
        }
        releaseKeyboard();
        m_ctx.context->mainWindow()->setFocus();
        close();
    }

protected:
    void focusOutEvent(QFocusEvent * event) override
    {
        releaseKeyboard();
        m_ctx.context->mainWindow()->setFocus();
        close();
    }
};


#endif //KSUITE_MULTISHORTCUT_H
