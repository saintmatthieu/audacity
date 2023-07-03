#pragma once
#pragma once

#include <NamedType/named_type.hpp>
#include <cassert>

using Beat = fluent::NamedType<double, struct BeatTag, fluent::Arithmetic>;
using BPS = fluent::NamedType<double, struct BPSTag, fluent::Arithmetic>;

inline double operator/(Beat beat, BPS bps)
{
   assert(bps.get() != 0);
   return beat.get() / bps.get();
}
