/*
 * Audacity: A Digital Audio Editor
 */

#pragma once

// This is required because this file is now used in effects/builtin
#include "effects/effects_base/effectstypes.h"

#include "framework/uicomponents/qml/Muse/UiComponents/menuitem.h"
#include "framework/audio/common/audiotypes.h"

namespace au::effects::utils {
using EffectFilter = std::function<bool (const EffectMeta&)>;

muse::uicomponents::MenuItemList destructiveEffectMenu(EffectMenuOrganization organization, EffectMetaList metaList,
                                                       const EffectFilter& filter, IEffectMenuItemFactory& effectMenu);

muse::uicomponents::MenuItemList realtimeEffectMenu(EffectMenuOrganization organization, EffectMetaList metaList,
                                                    const EffectFilter& filter, IEffectMenuItemFactory& effectMenu);

muse::String builtinEffectCategoryIdString(BuiltinEffectCategoryId category);

int builtinEffectCategoryIdOrder(const muse::String& category);

muse::audio::AudioResourceMeta toMuseAudioResourceMeta(const EffectMeta& effectMeta);

constexpr muse::audio::AudioResourceType toMuseAudioResourceType(EffectFamily family)
{
    switch (family) {
    case EffectFamily::Builtin: return muse::audio::AudioResourceType::MusePlugin; // hack for now
    case EffectFamily::VST3: return muse::audio::AudioResourceType::VstPlugin;
    case EffectFamily::LV2: return muse::audio::AudioResourceType::Lv2Plugin;
    case EffectFamily::AudioUnit: return muse::audio::AudioResourceType::AudioUnit;
    case EffectFamily::Nyquist: return muse::audio::AudioResourceType::NyquistPlugin;
    default:
        assert(false);
        return muse::audio::AudioResourceType::Undefined;
    }
}
}
