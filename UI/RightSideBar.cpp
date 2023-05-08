//
// Created by vr1s on 5/7/23.
//

#include "RightSideBar.h"
#include "binaryninjacore.h"
#include "ui/theme.h"

#include <QPainter>
#include <QStyleOptionButton>
#include <QDebug>
#include <QStylePainter>

OrientablePushButton::OrientablePushButton(QWidget *parent)
        : QPushButton(parent)
{ }

OrientablePushButton::OrientablePushButton(const QString &text, QWidget *parent)
        : QPushButton(text, parent)
{ }

OrientablePushButton::OrientablePushButton(const QIcon &icon, const QString &text, QWidget *parent)
        : QPushButton(icon, text, parent)
{ }

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

RightSideBar::RightSideBar(QWidget* parent)
{
    setMinimumSize(20, 300);
    setMouseTracking(true);

    QPalette pal = palette();
    pal.setColor(QPalette::Window, getThemeColor(SidebarBackgroundColor));
    setPalette(pal);
    setAutoFillBackground(true);

    auto layout = new QVBoxLayout();

    auto anotherButton = new OrientablePushButton("ARM Manual", this);
    anotherButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    anotherButton->setOrientation(OrientablePushButton::VerticalTopToBottom);
    anotherButton->setPalette(pal);
    anotherButton->setStyleSheet(QString("QPushButton {border: 0px;} "
                                         "QPushButton::hover {"
                                         "background-color: ") + getThemeColor(SidebarBackgroundColor).darker(140).name()
                                         + ";}");

    layout->addWidget(anotherButton);
    layout->setContentsMargins(0,0,0,0);
    layout->addStretch(1);
    setLayout(layout);
}
