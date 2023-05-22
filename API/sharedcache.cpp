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
    std::vector<std::string> SharedCache::GetAvailableImages()
    {
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
};