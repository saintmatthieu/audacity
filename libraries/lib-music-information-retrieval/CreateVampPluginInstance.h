#pragma once

#include <vamp-hostsdk/Plugin.h>

#include <memory>
#include <string>

namespace MIR
{
std::unique_ptr<Vamp::Plugin>
GetVampPluginInstance(const std::string& pluginKey, double sampleRate);
}
