/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include "modularity/imoduleinterface.h"

namespace au::effects {
class ILv2InstancesRegister : MODULE_EXPORT_INTERFACE
{
    INTERFACE_ID(ILv2InstancesRegister)
public:
    virtual ~ILv2InstancesRegister() = default;
};
}
