//
// Created by serket on 5/24/23.
//

#include <QGridLayout>
#include <QLabel>
#include "MultiShortcut.h"


MultiShortcut::MultiShortcut(UIActionContext* ctx, QWidget* parent)
{
    setFixedSize(500, 500);
    auto layout = new QGridLayout();
    for (int i = 0; i < 8; i++)
    {
        auto button = new QWidget(this);
        button->setLayout(new QVBoxLayout());
        auto nameLab = new QLabel();
        nameLab->setText(QString::fromStdString("Bind " + std::to_string(i)));
        button->layout()->addWidget(nameLab);
        auto keyLab = new QLabel();
        nameLab->setText(QString::fromStdString(std::to_string(i)));
        button->layout()->addWidget(keyLab);
        button->layout()->itemAt(1)->widget()->setStyleSheet("QLabel {opacity: 0.5;}");
        button->setFixedSize(500/3-20, 500/3-20);
        m_wheelItems.push_back(button);
        layout->addWidget(button, i / 3, i % 3);
    }
    setLayout(layout);

}

void MultiShortcut::setActionForItemIndex(size_t idx, MultiShortcutItem* item)
{
    assert(idx < 8);

    auto button = m_wheelItems.at(idx);
    auto nameLab = qobject_cast<QLabel*>(button->layout()->itemAt(0)->widget());
    nameLab->setText(item->text);
    auto bindLab = qobject_cast<QLabel*>(button->layout()->itemAt(1)->widget());
    bindLab->setText(QKeySequence(item->keybind->key()).toString());
}
