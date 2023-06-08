//
// Created by serket on 5/24/23.
//

#include <QGridLayout>
#include <QLabel>
#include "MultiShortcut.h"


MultiShortcut::MultiShortcut(UIActionContext ctx, QWidget* parent)
    : QWidget(parent), m_ctx(ctx)
{
    //installEventFilter(this);
    setObjectName("multiShortcut");
    setWindowFlags(Qt::FramelessWindowHint | Qt::Popup);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_TranslucentBackground);
    setStyleSheet("QWidget {"
                  "border-radius: 7px;"
                  "background-color: #202020;"
                  "}");
    auto layout = new QGridLayout();
    for (int i = 0; i < 9; i++)
    {
        m_actions.push_back(nullptr);
        if (i == 4)
            continue;
        auto button = new QWidget(this);
        button->setObjectName("button" + std::to_string(i));
        button->setLayout(new QVBoxLayout());
        auto nameLab = new QLabel();
        nameLab->setText(QString::fromStdString("Bind " + std::to_string(i)));
        nameLab->setAlignment(Qt::AlignCenter);
        button->layout()->addWidget(nameLab);
        auto keyLab = new QLabel();
        keyLab->setText(QString::fromStdString(std::to_string(i)));
        keyLab->setStyleSheet("QLabel {opacity: 0.9;}");
        keyLab->setAlignment(Qt::AlignCenter);
        button->layout()->addWidget(keyLab);
        //button->layout()->itemAt(1)->widget()->setStyleSheet("QLabel {opacity: 0.9;}");
        button->setFixedSize(100, 100);
        button->setVisible(false);
        m_wheelItems.push_back(button);
        layout->addWidget(button, i / 3, i % 3);
    }
    setLayout(layout);

}

void MultiShortcut::setActionForItemIndex(size_t idx, MultiShortcutItem* item)
{
    assert(idx < 8);

    m_actions[idx] = item;
    auto button = m_wheelItems.at(idx);
    button->setVisible(true);
    auto nameLab = qobject_cast<QLabel*>(button->layout()->itemAt(0)->widget());
    nameLab->setText(item->text);
    auto bindLab = qobject_cast<QLabel*>(button->layout()->itemAt(1)->widget());
    bindLab->setText(QKeySequence(item->keybind->key()).toString());
}
