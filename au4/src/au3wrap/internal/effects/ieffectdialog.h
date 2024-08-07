#pragma once

namespace au::au3
{
class IEffectDialog
{
public:
    virtual ~IEffectDialog() = default;
    virtual bool Show() = 0;
};
} // namespace au::au3
