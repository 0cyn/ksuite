//
// Created by vr1s on 5/8/23.
//
#include "uicontext.h"

#ifndef KSUITE_NOTIFICATIONS_H
#define KSUITE_NOTIFICATIONS_H

class ContextSidebarManager;


class Notifications : public UIContextNotification {
    static Notifications* m_instance;

    std::unordered_map<UIContext*, ContextSidebarManager*> m_ctxForSidebar;

    std::vector<size_t> m_sessionsAlreadyDisplayedPickerFor;

public:
    virtual void OnContextOpen(UIContext* context) override;
    virtual void OnContextClose(UIContext* context) override;
    virtual void OnViewChange(UIContext *context, ViewFrame *frame, const QString &type);
    static void init();
};


#endif //KSUITE_NOTIFICATIONS_H
