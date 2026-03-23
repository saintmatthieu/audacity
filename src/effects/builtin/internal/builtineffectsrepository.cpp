/*
* Audacity: A Digital Audio Editor
*/
#include "builtineffectsrepository.h"

#include "framework/global/log.h"
#include "effects/effects_base/effectstypes.h"

#include <QtQml>
#include <algorithm>

using namespace au::effects;

void BuiltinEffectsRepository::init()
{
    m_metas.clear();
    for (const auto& loader : m_loaders) {
        EffectMetaList metas = loader->effectMetaList();
        m_metas.insert(m_metas.end(), std::make_move_iterator(metas.begin()), std::make_move_iterator(metas.end()));
    }
    m_effectMetaListUpdated.notify();
}

void BuiltinEffectsRepository::registerLoader(const std::shared_ptr<IBuiltinEffectsLoader>& loader)
{
    IF_ASSERT_FAILED(std::find(m_loaders.begin(), m_loaders.end(), loader) == m_loaders.end()) {
        LOGW() << "Trying to register duplicate loader";
        return;
    }
    loader->effectMetaListUpdated().onNotify(this, [this]() {
        init();
    });
    m_loaders.push_back(loader);
    init();
}

muse::async::Notification BuiltinEffectsRepository::effectMetaListUpdated() const
{
    return m_effectMetaListUpdated;
}

EffectMetaList BuiltinEffectsRepository::effectMetaList() const
{
    return m_metas;
}
