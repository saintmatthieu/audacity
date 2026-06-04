/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <vector>
#include <string>

#include <QString>

#include "modularity/imoduleinterface.h"
#include "global/types/translatablestring.h"
#include "actions/actiontypes.h"

namespace au::au3cloud {
//! A shipped (non-plugin) cloud effect.
struct CloudEffectItem {
    std::string id;                   //!< stable id, used in the open action
    muse::TranslatableString title;   //!< menu / UI title
    QString qmlUrl;                   //!< url of the CloudEffect-derived QML, loaded in the dialog
};

//! Provides the shipped cloud effects. Prototype: a hardcoded list (see
//! CloudEffectsProvider). Replaced later by real registration.
class ICloudEffectsProvider : MODULE_GLOBAL_INTERFACE
{
    INTERFACE_ID(ICloudEffectsProvider)
public:
    virtual ~ICloudEffectsProvider() = default;

    virtual const std::vector<CloudEffectItem>& effects() const = 0;
};

//! Action code that opens the given cloud effect.
inline muse::actions::ActionCode cloudEffectOpenActionCode(const std::string& id)
{
    return "action://cloudeffects/open?effectId=" + id;
}

//! Dialog uri, registered to CloudEffectDialog.qml.
inline const std::string CLOUD_EFFECT_DIALOG_URI = "audacity://cloudeffects/dialog";
}
