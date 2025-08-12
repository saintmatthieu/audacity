/*
 * Audacity: A Digital Audio Editor
 */

#include "controlwarper.h"

#include "global/types/number.h"

#include <cmath>

namespace au::effects {
namespace {
struct Normalized {
    Normalized(double v, double min, double max)
        : value{(std::clamp(v, min, max) - min) / (max - min)} {}
    const double value;
};

struct Expanded {
    Expanded(double v, double min, double max)
        : value{v* (max - min) + min} {}
    const double value;
};

/*!
 * We use a warping function that maps f(min) = min and f(max) = max.
 * For simpler maths, we first normalize the warped value to [0, 1] range.
 * Then we use `y(x) = (exp(C * x) - 1) / (exp(C) - 1)`.
 * `softC` is solving this equation for `y(1/2) = 1/4`, meaning that when the control is at 50%, the warped value is at 25%.
 * `aggressiveC` is solving this equation so that when the control is at 50%, the warped value is at 12.5%.
 */
constexpr double softC = 2.1972245773362196;
constexpr double aggressiveC = 3.8918202981106265;
static const double softExpC = std::exp(softC);
static const double aggressiveExpC = std::exp(aggressiveC);
}

void ControlWarper::init()
{
    if (m_initialized) {
        return;
    }

    m_initialized = true;
    emit valueChanged();
}

double ControlWarper::value() const
{
    return m_value;
}

void ControlWarper::setValue(double value)
{
    if (muse::is_equal(value, m_value)) {
        return;
    }
    m_value = value;

    if (m_initialized) {
        emit valueChanged();
        if (updateWarpedValue()) {
            emit warpedValueChanged();
        }
    }
}

bool ControlWarper::updateWarpedValue()
{
    if (m_warpingType == ControlWarpingType::None) {
        m_warpedValue = m_value;
        return true;
    }

    if (m_min > m_max || muse::is_equal(m_min, m_max)) {
        return false;
    }

    const auto m = m_min;
    const auto M = m_max;
    const Normalized y{ m_value, m, M };
    const auto C = m_warpingType == ControlWarpingType::Aggressive ? aggressiveC : softC;
    const auto expC = m_warpingType == ControlWarpingType::Aggressive ? aggressiveExpC : softExpC;
    // The inverse of the warping function, on the domain [0, 1] ...
    const auto u = 1 / C * std::log(y.value * (expC - 1.0) + 1.0);
    // ... then expand back to the original range.
    const Expanded x{ u, m, M };
    m_warpedValue = x.value;

    return true;
}

double ControlWarper::warpedValue() const
{
    return m_warpedValue;
}

void ControlWarper::setWarpedValue(double value)
{
    if (muse::is_equal(m_warpedValue, value)) {
        return;
    }
    m_warpedValue = value;
    if (m_initialized) {
        emit warpedValueChanged();
        if (updateValue()) {
            emit valueChanged();
        }
    }
}

bool ControlWarper::updateValue()
{
    if (m_warpingType == ControlWarpingType::None) {
        m_value = m_warpedValue;
        return true;
    }

    if (m_min > m_max || muse::is_equal(m_min, m_max)) {
        return false;
    }

    const auto m = m_min;
    const auto M = m_max;
    const Normalized x{ m_warpedValue, m, M };
    const auto C = m_warpingType == ControlWarpingType::Aggressive ? aggressiveC : softC;
    const auto expC = m_warpingType == ControlWarpingType::Aggressive ? aggressiveExpC : softExpC;
    const auto u = (std::exp(C * x.value) - 1.0) / (expC - 1.0);
    const Expanded y{ u, m, M };
    m_value = y.value;

    return true;
}

ControlWarpingType ControlWarper::warpingType() const
{
    return m_warpingType;
}

void ControlWarper::setWarpingType(ControlWarpingType type)
{
    if (m_warpingType == type) {
        return;
    }
    m_warpingType = type;
    emit warpingTypeChanged();
    if (m_initialized && updateValue()) {
        emit valueChanged();
    }
}

double ControlWarper::min() const
{
    return m_min;
}

void ControlWarper::setMin(double value)
{
    if (muse::is_equal(m_min, value)) {
        return;
    }
    m_min = value;
    emit minChanged();
    if (m_initialized && updateValue()) {
        emit valueChanged();
    }
}

double ControlWarper::max() const
{
    return m_max;
}

void ControlWarper::setMax(double value)
{
    if (muse::is_equal(m_max, value)) {
        return;
    }
    m_max = value;
    emit maxChanged();
    if (m_initialized && updateValue()) {
        emit valueChanged();
    }
}
}
