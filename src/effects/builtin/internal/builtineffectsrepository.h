/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include "../ibuiltineffectsrepository.h"

#include "framework/global/async/asyncable.h"

#include <vector>

namespace au::effects {
class BuiltinEffectsRepository : public IBuiltinEffectsRepository, public muse::async::Asyncable
{
public:
    void init();

    muse::async::Notification effectMetaListUpdated() const override;
    EffectMetaList effectMetaList() const override;
    void registerLoader(const std::shared_ptr<IBuiltinEffectsLoader>& loader) override;

private:
    muse::async::Notification m_effectMetaListUpdated;
    EffectMetaList m_metas;
    std::vector<std::shared_ptr<IBuiltinEffectsLoader> > m_loaders;
};
}
