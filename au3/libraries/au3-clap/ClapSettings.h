/**********************************************************************

  Audacity: A Digital Audio Editor

  @file ClapSettings.h

  @brief Persistent state of a CLAP effect, stored inside au3 EffectSettings.

**********************************************************************/
#pragma once

#include <cstdint>
#include <map>
#include <vector>

//! Persistent CLAP effect state held inside the type-erased au3 EffectSettings.
//!
//! Two representations coexist:
//!  - \p values keeps the plain parameter values keyed by their stable CLAP id.
//!    They are used by the auto-generated UI and as a fallback when the plugin
//!    does not implement the clap.state extension.
//!  - \p chunk is the opaque blob produced by the clap.state extension. When it
//!    is non-empty it is authoritative and fully restores the plugin state.
struct ClapEffectSettings {
    std::map<uint32_t, double> values;
    std::vector<uint8_t> chunk;
};
