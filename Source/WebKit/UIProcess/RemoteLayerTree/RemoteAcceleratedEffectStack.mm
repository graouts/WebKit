/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "RemoteAcceleratedEffectStack.h"

#if ENABLE(THREADED_ANIMATION_RESOLUTION)

#import <pal/spi/cocoa/QuartzCoreSPI.h>
#include <wtf/IsoMallocInlines.h>

namespace WebKit {

WTF_MAKE_ISO_ALLOCATED_IMPL(RemoteAcceleratedEffectStack);

Ref<RemoteAcceleratedEffectStack> RemoteAcceleratedEffectStack::create(WebCore::FloatRect bounds, Seconds acceleratedTimelineTimeOrigin)
{
    return adoptRef(*new RemoteAcceleratedEffectStack(bounds, acceleratedTimelineTimeOrigin));
}

RemoteAcceleratedEffectStack::RemoteAcceleratedEffectStack(WebCore::FloatRect bounds, Seconds acceleratedTimelineTimeOrigin)
    : m_bounds(bounds)
    , m_acceleratedTimelineTimeOrigin(acceleratedTimelineTimeOrigin)
{
}

void RemoteAcceleratedEffectStack::setEffects(AcceleratedEffects&& effects) {
    AcceleratedEffectStack::setEffects(WTFMove(effects));

    for (auto& effect : m_backdropLayerEffects.isEmpty() ? m_primaryLayerEffects : m_backdropLayerEffects) {
        auto& properties = effect->animatedProperties();
        m_animatesFilter = m_animatesFilter || properties.containsAny({ AcceleratedEffectProperty::Filter, AcceleratedEffectProperty::BackdropFilter });
        m_animatesOpacity = m_animatesOpacity || properties.contains(AcceleratedEffectProperty::Opacity);
        m_animatesTransform = m_animatesTransform || properties.containsAny(transformRelatedAcceleratedProperties);
        if (m_animatesFilter && m_animatesOpacity && m_animatesTransform)
            break;
    }

    ASSERT(m_animatesFilter || m_animatesOpacity || m_animatesTransform);
}

#if PLATFORM(MAC)
const WebCore::FilterOperations* RemoteAcceleratedEffectStack::longestFilter() const
{
    if (!m_animatesFilter)
        return nullptr;

    auto isBackdrop = !m_backdropLayerEffects.isEmpty();
    auto filterProperty = isBackdrop ? AcceleratedEffectProperty::BackdropFilter : AcceleratedEffectProperty::Filter;
    auto& effects = isBackdrop ? m_backdropLayerEffects : m_primaryLayerEffects;

    const WebCore::FilterOperations* longestFilter = nullptr;
    for (auto& effect : effects) {
        if (!effect->animatedProperties().contains(filterProperty))
            continue;
        for (auto& keyframe : effect->keyframes()) {
            if (!keyframe.animatedProperties().contains(filterProperty))
                continue;
            auto& filter = isBackdrop ? keyframe.values().backdropFilter : keyframe.values().filter;
            if (!longestFilter || longestFilter->size() < filter.size())
                longestFilter = &filter;
        }
    }

    if (longestFilter) {
        auto& baseFilter = isBackdrop ? m_baseValues.backdropFilter : m_baseValues.filter;
        if (longestFilter->size() < baseFilter.size())
            longestFilter = &baseFilter;
    }

    return longestFilter && !longestFilter->isEmpty() ? longestFilter : nullptr;
}

void RemoteAcceleratedEffectStack::initEffectsFromMainThread(PlatformLayer *layer, MonotonicTime now)
{
    ASSERT(m_filterPresentationModifiers.isEmpty());
    ASSERT(!m_opacityPresentationModifier);
    ASSERT(!m_transformPresentationModifier);
    ASSERT(!m_presentationModifierGroup);

    auto computedValues = computeValues(now);

    auto* canonicalFilters = longestFilter();

    auto numberOfPresentationModifiers = [&]() {
        size_t count = 0;
        if (m_animatesFilter) {
            ASSERT(canonicalFilters);
            count += PlatformCAFilters::presentationModifierCountForFilters(*canonicalFilters);
        }
        if (m_animatesOpacity)
            count++;
        if (m_animatesTransform)
            count++;
        return count;
    }();

    m_presentationModifierGroup = [CAPresentationModifierGroup groupWithCapacity:numberOfPresentationModifiers];

    if (m_animatesFilter) {
        PlatformCAFilters::presentationModifiersForFilters(computedValues.filter, longestFilter(), m_filterPresentationModifiers, m_presentationModifierGroup);
        for (auto& filterPresentationModifier : m_filterPresentationModifiers)
            [layer addPresentationModifier:filterPresentationModifier.second.get()];
    }

    if (m_animatesOpacity) {
        auto *opacity = @(computedValues.opacity);
        m_opacityPresentationModifier = adoptNS([[CAPresentationModifier alloc] initWithKeyPath:@"opacity" initialValue:opacity additive:NO group:m_presentationModifierGroup.get()]);
        [layer addPresentationModifier:m_opacityPresentationModifier.get()];
    }

    if (m_animatesTransform) {
        auto computedTransform = computedValues.computedTransformationMatrix(m_bounds);
        auto *transform = [NSValue valueWithCATransform3D:computedTransform];
        m_transformPresentationModifier = adoptNS([[CAPresentationModifier alloc] initWithKeyPath:@"transform" initialValue:transform additive:NO group:m_presentationModifierGroup.get()]);
        [layer addPresentationModifier:m_transformPresentationModifier.get()];
    }

    [m_presentationModifierGroup flushWithTransaction];
}

void RemoteAcceleratedEffectStack::applyEffectsFromScrollingThread(MonotonicTime now) const
{
    ASSERT(m_presentationModifierGroup);

    auto computedValues = computeValues(now);

    if (!m_filterPresentationModifiers.isEmpty())
        PlatformCAFilters::updatePresentationModifiersForFilters(computedValues.filter, m_filterPresentationModifiers);

    if (m_opacityPresentationModifier) {
        auto *opacity = @(computedValues.opacity);
        [m_opacityPresentationModifier setValue:opacity];
    }

    if (m_transformPresentationModifier) {
        auto computedTransform = computedValues.computedTransformationMatrix(m_bounds);
        auto *transform = [NSValue valueWithCATransform3D:computedTransform];
        [m_transformPresentationModifier setValue:transform];
    }

    [m_presentationModifierGroup flush];
}
#endif

void RemoteAcceleratedEffectStack::applyEffectsFromMainThread(PlatformLayer *layer, MonotonicTime now) const
{
    auto computedValues = computeValues(now);

    if (m_animatesFilter)
        PlatformCAFilters::setFiltersOnLayer(layer, computedValues.filter);

    if (m_animatesOpacity)
        [layer setOpacity:computedValues.opacity];

    if (m_animatesTransform) {
        auto computedTransform = computedValues.computedTransformationMatrix(m_bounds);
        [layer setTransform:computedTransform];
    }
}

AcceleratedEffectValues RemoteAcceleratedEffectStack::computeValues(MonotonicTime now) const
{
    auto values = m_baseValues;
    auto currentTime = now.secondsSinceEpoch() - m_acceleratedTimelineTimeOrigin;
    for (auto& effect : m_backdropLayerEffects.isEmpty() ? m_primaryLayerEffects : m_backdropLayerEffects)
        effect->apply(currentTime, values, m_bounds);
    return values;
}

void RemoteAcceleratedEffectStack::clear(PlatformLayer *layer)
{
    ASSERT(m_presentationModifierGroup);

    for (auto& filterPresentationModifier : m_filterPresentationModifiers)
        [layer removePresentationModifier:filterPresentationModifier.second.get()];
    if (m_opacityPresentationModifier)
        [layer removePresentationModifier:m_opacityPresentationModifier.get()];
    if (m_transformPresentationModifier)
        [layer removePresentationModifier:m_transformPresentationModifier.get()];

    [m_presentationModifierGroup flushWithTransaction];

    m_filterPresentationModifiers.clear();
    m_opacityPresentationModifier = nil;
    m_transformPresentationModifier = nil;
    m_presentationModifierGroup = nil;
}

} // namespace WebKit

#endif // ENABLE(THREADED_ANIMATION_RESOLUTION)
