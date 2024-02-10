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

#if PLATFORM(MAC)
void RemoteAcceleratedEffectStack::initEffectsFromMainThread(PlatformLayer *layer, MonotonicTime now)
{
    ASSERT(!m_opacityPresentationModifier);
    ASSERT(!m_transformPresentationModifier);

    auto computedValues = computeValues(now);
    auto computedTransform = computedValues.computedTransformationMatrix(m_bounds);

    auto *opacity = @(computedValues.opacity);
    auto *transform = [NSValue valueWithCATransform3D:computedTransform];

    m_presentationModifierGroup = [CAPresentationModifierGroup groupWithCapacity:2];
    m_opacityPresentationModifier = adoptNS([[CAPresentationModifier alloc] initWithKeyPath:@"opacity" initialValue:opacity additive:NO group:m_presentationModifierGroup.get()]);
    m_transformPresentationModifier = adoptNS([[CAPresentationModifier alloc] initWithKeyPath:@"transform" initialValue:transform additive:NO group:m_presentationModifierGroup.get()]);

    [layer addPresentationModifier:m_opacityPresentationModifier.get()];
    [layer addPresentationModifier:m_transformPresentationModifier.get()];

    [m_presentationModifierGroup flushWithTransaction];
}

void RemoteAcceleratedEffectStack::applyEffectsFromScrollingThread(MonotonicTime now) const
{
    auto computedValues = computeValues(now);
    auto computedTransform = computedValues.computedTransformationMatrix(m_bounds);

    auto *opacity = @(computedValues.opacity);
    auto *transform = [NSValue valueWithCATransform3D:computedTransform];

    [m_opacityPresentationModifier setValue:opacity];
    [m_transformPresentationModifier setValue:transform];
    [m_presentationModifierGroup flush];
}
#endif

void RemoteAcceleratedEffectStack::applyEffectsFromMainThread(PlatformLayer *layer, MonotonicTime now) const
{
    auto computedValues = computeValues(now);
    auto computedTransform = computedValues.computedTransformationMatrix(m_bounds);

    [layer setOpacity:computedValues.opacity];
    [layer setTransform:computedTransform];
}

AcceleratedEffectValues RemoteAcceleratedEffectStack::computeValues(MonotonicTime now) const
{
    auto values = m_baseValues;
    auto currentTime = now.secondsSinceEpoch() - m_acceleratedTimelineTimeOrigin;
    for (auto& effect : m_primaryLayerEffects)
        effect->apply(currentTime, values, m_bounds);
    return values;
}

void RemoteAcceleratedEffectStack::clear(PlatformLayer *layer)
{
    if (!m_presentationModifierGroup) {
        ASSERT(!m_opacityPresentationModifier);
        ASSERT(!m_transformPresentationModifier);
        return;
    }

    ASSERT(m_opacityPresentationModifier);
    ASSERT(m_transformPresentationModifier);

    [layer removePresentationModifier:m_opacityPresentationModifier.get()];
    [layer removePresentationModifier:m_transformPresentationModifier.get()];
    [m_presentationModifierGroup flushWithTransaction];

    m_opacityPresentationModifier = nil;
    m_transformPresentationModifier = nil;
    m_presentationModifierGroup = nil;
}

} // namespace WebKit

#endif // ENABLE(THREADED_ANIMATION_RESOLUTION)
