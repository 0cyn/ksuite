//
// Created by vr1s on 5/7/23.
//

#ifndef KSUITE_RIGHTSIDEBAR_H
#define KSUITE_RIGHTSIDEBAR_H


#include <QtWidgets>

class OrientablePushButton : public QPushButton
{
Q_OBJECT
public:
    enum Orientation {
        Horizontal,
        VerticalTopToBottom,
        VerticalBottomToTop
    };

    OrientablePushButton(QWidget * parent = nullptr);
    OrientablePushButton(const QString & text, QWidget *parent = nullptr);
    OrientablePushButton(const QIcon & icon, const QString & text, QWidget *parent = nullptr);

    QSize sizeHint() const;

    OrientablePushButton::Orientation orientation() const;
    void setOrientation(const OrientablePushButton::Orientation &orientation);

protected:
    void paintEvent(QPaintEvent *event);

private:
    Orientation mOrientation = Horizontal;
};
class RightSideBar : public QWidget {
    Q_OBJECT
public:

    RightSideBar(QWidget* parent);
};


#endif //KSUITE_RIGHTSIDEBAR_H
