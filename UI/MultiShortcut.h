//
// Created by serket on 5/24/23.
//

#ifndef KSUITE_MULTISHORTCUT_H
#define KSUITE_MULTISHORTCUT_H


#include <QWidget>
#include "action.h"

struct MultiShortcutItem;

class MultiShortcut : public QWidget {

    Q_OBJECT

    std::vector<MultiShortcutItem*> m_actions;
    std::vector<QWidget*> m_wheelItems;
public:
    MultiShortcut(UIActionContext ctx, QWidget* parent);
    struct MultiShortcutItem {
        UIAction* action;
        QKeyCombination* keybind;
        QString text;
    };
    void setActionForItemIndex(size_t idx, MultiShortcutItem* item);

protected:
    void focusOutEvent(QFocusEvent * event) override
    {
        close();
    }
};


#endif //KSUITE_MULTISHORTCUT_H
