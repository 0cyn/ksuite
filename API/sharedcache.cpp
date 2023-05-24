//
// Created by kat on 5/21/23.
//
//
// Created by kat on 4/24/23.
//

#include "ksuiteapi.h"
#include "ksuitecore.h"

namespace KAPI {

    SharedCache::SharedCache(Ref<BinaryView> view) : m_view(view)
    {}
    bool SharedCache::LoadImageWithInstallName(std::string installName)
    {
        if (!m_view->GetParentView())
            return false;
        char* str = BNAllocString(installName.c_str());
        return BNDSCViewLoadImageWithInstallName(m_view->m_object, str);
    }
    bool SharedCache::LoadSectionAtAddress(uint64_t addr)
    {
        if (!m_view->GetParentView())
            return false;
        return BNDSCViewLoadSectionAtAddress(m_view->m_object, addr);
    }
    std::vector<std::string> SharedCache::GetAvailableImages()
    {
        if (!m_view->GetParentView())
            return {};
        size_t count;
        char** value = BNDSCViewGetInstallNames(m_view->m_object, &count);
        if (value == nullptr)
        {
            return {};
        }

        std::vector<std::string> result;
        for (size_t i = 0; i < count; i++)
        {
            result.push_back(value[i]);
        }

        BNFreeStringList(value, count);
        return result;
    }
    uint64_t SharedCache::LoadedImageCount()
    {
        if (!m_view->GetParentView())
            return {};
        return BNDSCViewLoadedImageCount(m_view->m_object);
    }
};