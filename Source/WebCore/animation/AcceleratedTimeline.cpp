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

#include "config.h"
#include "AcceleratedTimeline.h"

#if ENABLE(THREADED_ANIMATION_RESOLUTION)

#include "Document.h"
#include "LocalDOMWindow.h"
#include "GraphicsLayerCA.h"
#include "Page.h"
#include "Performance.h"
#include "RenderElement.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderLayerModelObject.h"
#include "RenderStyleConstants.h"
#include "Styleable.h"
#include "ScrollingThread.h"
#include <wtf/MonotonicTime.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

HashSet<AcceleratedTimeline*>& AcceleratedTimeline::timelines()
{
    static NeverDestroyed<HashSet<AcceleratedTimeline*>> vector;
    return vector;
}

void AcceleratedTimeline::updateTimelinesForPageId(PageIdentifier pageId, const Function<void(bool)>&& callback)
{
    LOG_WITH_STREAM(Animations, stream << "AcceleratedTimeline::updateTimelinesForPageId(" << pageId << ")");
    ScrollingThread::dispatch([pageId, callback = WTFMove(callback)] {
        auto shouldKeepUpdating = false;

        for (auto* acceleratedTimeline : timelines()) {
            if (!acceleratedTimeline || acceleratedTimeline->m_pageId != pageId)
                continue;

            acceleratedTimeline->update();
            if (!shouldKeepUpdating && acceleratedTimeline->shouldScheduleUpdates())
                shouldKeepUpdating = true;
        }

        if (shouldKeepUpdating)
            return;

        ScrollingThread::dispatchBarrier([shouldKeepUpdating, callback = WTFMove(callback)] {
            callback(shouldKeepUpdating);
        });
    });
}

AcceleratedTimeline::AcceleratedTimeline(Document& document)
{
    ASSERT(isMainThread());
    auto now = MonotonicTime::now();
    m_timeOrigin = now.secondsSinceEpoch();
    if (auto* domWindow = document.domWindow())
        m_timeOrigin -= Seconds::fromMilliseconds(domWindow->performance().relativeTimeFromTimeOriginInReducedResolution(now));
    
    ASSERT(document.pageID());
    m_pageId = *document.pageID();
    timelines().add(this);
}

void AcceleratedTimeline::updateEffectStacks()
{
    ASSERT(isMainThread());
    auto targetsPendingUpdate = std::exchange(m_targetsPendingUpdate, { });
    for (auto hashedStyleable : targetsPendingUpdate) {
        auto* element = hashedStyleable.first;
        if (!element)
            continue;

        auto pseudoId = static_cast<PseudoId>(hashedStyleable.second);
        Styleable target { *element, pseudoId };

        auto* renderer = target.renderer();
        if (!renderer || !renderer->isComposited() || !is<RenderLayerModelObject>(renderer))
            continue;

        auto* renderLayer = downcast<RenderLayerModelObject>(*renderer).layer();
        ASSERT(renderLayer && renderLayer->backing() && renderLayer->backing()->graphicsLayer());
        auto& renderLayerBacking = *renderLayer->backing();

        auto hasRunningAcceleratedEffects = renderLayerBacking.updateAcceleratedEffectsAndBaseValues();
        if (hasRunningAcceleratedEffects)
            m_animatedGraphicsLayers.add(renderLayerBacking.graphicsLayer());
        else
            m_animatedGraphicsLayers.remove(renderLayerBacking.graphicsLayer());
    }
}

void AcceleratedTimeline::updateEffectStackForTarget(const Styleable& target)
{
    ASSERT(isMainThread());
    m_targetsPendingUpdate.add({ &target.element, static_cast<unsigned>(target.pseudoId) });
}

bool AcceleratedTimeline::shouldScheduleUpdates() const
{
    return !m_animatedGraphicsLayers.isEmpty();
}

void AcceleratedTimeline::update()
{
    ASSERT(!isMainThread());

    auto currentTime = MonotonicTime::now().secondsSinceEpoch() - m_timeOrigin;
    for (auto* animatedGraphicsLayer : m_animatedGraphicsLayers) {
        if (auto* acceleratedEffectStack = animatedGraphicsLayer->acceleratedEffectStack()) {
            ASSERT(is<GraphicsLayerCA>(animatedGraphicsLayer));
            auto& graphicsLayerCA = downcast<GraphicsLayerCA>(*animatedGraphicsLayer);
            auto *primaryLayer = graphicsLayerCA.animatedLayer(AnimatedProperty::Transform)->platformLayer();
            acceleratedEffectStack->applyPrimaryLayerEffects(primaryLayer, currentTime);
            auto *backdropLayer = graphicsLayerCA.animatedLayer(AnimatedProperty::WebkitBackdropFilter)->platformLayer();
            acceleratedEffectStack->applyBackdropLayerEffects(backdropLayer, currentTime);
        }
    }
}

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATION_RESOLUTION)
