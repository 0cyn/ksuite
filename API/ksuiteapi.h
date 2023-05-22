#pragma once

#include <binaryninjaapi.h>

using namespace BinaryNinja;

namespace KAPI {
    template<class T>
    class KRefCountObject {
        void AddRefInternal() { m_refs.fetch_add(1); }

        void ReleaseInternal() {
            if (m_refs.fetch_sub(1) == 1)
                delete this;
        }

    public:
        std::atomic<int> m_refs;
        T *m_object;

        KRefCountObject() : m_refs(0), m_object(nullptr) {}

        virtual ~KRefCountObject() {}

        T *GetObject() const { return m_object; }

        static T *GetObject(KRefCountObject *obj) {
            if (!obj)
                return nullptr;
            return obj->GetObject();
        }

        void AddRef() { AddRefInternal(); }

        void Release() { ReleaseInternal(); }

        void AddRefForRegistration() { AddRefInternal(); }
    };


    template<class T, T *(*AddObjectReference)(T *), void (*FreeObjectReference)(T *)>
    class KCoreRefCountObject {
        void AddRefInternal() { m_refs.fetch_add(1); }

        void ReleaseInternal() {
            if (m_refs.fetch_sub(1) == 1) {
                if (!m_registeredRef)
                    delete this;
            }
        }

    public:
        std::atomic<int> m_refs;
        bool m_registeredRef = false;
        T *m_object;

        KCoreRefCountObject() : m_refs(0), m_object(nullptr) {}

        virtual ~KCoreRefCountObject() {}

        T *GetObject() const { return m_object; }

        static T *GetObject(KCoreRefCountObject *obj) {
            if (!obj)
                return nullptr;
            return obj->GetObject();
        }

        void AddRef() {
            if (m_object && (m_refs != 0))
                AddObjectReference(m_object);
            AddRefInternal();
        }

        void Release() {
            if (m_object)
                FreeObjectReference(m_object);
            ReleaseInternal();
        }

        void AddRefForRegistration() { m_registeredRef = true; }

        void ReleaseForRegistration() {
            m_object = nullptr;
            m_registeredRef = false;
            if (m_refs == 0)
                delete this;
        }
    };


    template<class T>
    class KRef {
        T *m_obj;
#ifdef BN_REF_COUNT_DEBUG
        void* m_assignmentTrace = nullptr;
#endif

    public:
        KRef<T>() : m_obj(NULL) {}

        KRef<T>(T *obj) : m_obj(obj) {
            if (m_obj) {
                m_obj->AddRef();
#ifdef BN_REF_COUNT_DEBUG
                m_assignmentTrace = BNRegisterObjectRefDebugTrace(typeid(T).name());
#endif
            }
        }

        KRef<T>(const KRef<T> &obj) : m_obj(obj.m_obj) {
            if (m_obj) {
                m_obj->AddRef();
#ifdef BN_REF_COUNT_DEBUG
                m_assignmentTrace = BNRegisterObjectRefDebugTrace(typeid(T).name());
#endif
            }
        }

        KRef<T>(KRef<T> &&other) : m_obj(other.m_obj) {
            other.m_obj = 0;
#ifdef BN_REF_COUNT_DEBUG
            m_assignmentTrace = other.m_assignmentTrace;
#endif
        }

        ~KRef<T>() {
            if (m_obj) {
                m_obj->Release();
#ifdef BN_REF_COUNT_DEBUG
                BNUnregisterObjectRefDebugTrace(typeid(T).name(), m_assignmentTrace);
#endif
            }
        }

        KRef<T> &operator=(const Ref<T> &obj) {
#ifdef BN_REF_COUNT_DEBUG
            if (m_obj)
                BNUnregisterObjectRefDebugTrace(typeid(T).name(), m_assignmentTrace);
            if (obj.m_obj)
                m_assignmentTrace = BNRegisterObjectRefDebugTrace(typeid(T).name());
#endif
            T *oldObj = m_obj;
            m_obj = obj.m_obj;
            if (m_obj)
                m_obj->AddRef();
            if (oldObj)
                oldObj->Release();
            return *this;
        }

        KRef<T> &operator=(KRef<T> &&other) {
            if (m_obj) {
#ifdef BN_REF_COUNT_DEBUG
                BNUnregisterObjectRefDebugTrace(typeid(T).name(), m_assignmentTrace);
#endif
                m_obj->Release();
            }
            m_obj = other.m_obj;
            other.m_obj = 0;
#ifdef BN_REF_COUNT_DEBUG
            m_assignmentTrace = other.m_assignmentTrace;
#endif
            return *this;
        }

        KRef<T> &operator=(T *obj) {
#ifdef BN_REF_COUNT_DEBUG
            if (m_obj)
                BNUnregisterObjectRefDebugTrace(typeid(T).name(), m_assignmentTrace);
            if (obj)
                m_assignmentTrace = BNRegisterObjectRefDebugTrace(typeid(T).name());
#endif
            T *oldObj = m_obj;
            m_obj = obj;
            if (m_obj)
                m_obj->AddRef();
            if (oldObj)
                oldObj->Release();
            return *this;
        }

        operator T *() const {
            return m_obj;
        }

        T *operator->() const {
            return m_obj;
        }

        T &operator*() const {
            return *m_obj;
        }

        bool operator!() const {
            return m_obj == NULL;
        }

        bool operator==(const T *obj) const {
            return T::GetObject(m_obj) == T::GetObject(obj);
        }

        bool operator==(const KRef<T> &obj) const {
            return T::GetObject(m_obj) == T::GetObject(obj.m_obj);
        }

        bool operator!=(const T *obj) const {
            return T::GetObject(m_obj) != T::GetObject(obj);
        }

        bool operator!=(const KRef<T> &obj) const {
            return T::GetObject(m_obj) != T::GetObject(obj.m_obj);
        }

        bool operator<(const T *obj) const {
            return T::GetObject(m_obj) < T::GetObject(obj);
        }

        bool operator<(const KRef<T> &obj) const {
            return T::GetObject(m_obj) < T::GetObject(obj.m_obj);
        }

        T *GetPtr() const {
            return m_obj;
        }
    };

    class SharedCache {
        Ref<BinaryView> m_view;
    public:
        SharedCache(Ref<BinaryView> view);


        bool LoadImageWithInstallName(std::string installName);
        std::vector<std::string> GetAvailableImages();
    };
}