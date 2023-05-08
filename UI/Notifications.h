//
// Created by vr1s on 5/8/23.
//
#include "uicontext.h"

#ifndef KSUITE_NOTIFICATIONS_H
#define KSUITE_NOTIFICATIONS_H


class Notifications : public UIContextNotification {
    static Notifications* m_instance;
public:
    virtual void OnContextOpen(UIContext* context) override;
    virtual void OnContextClose(UIContext* context) override;

    static void init();
};


#endif //KSUITE_NOTIFICATIONS_H
