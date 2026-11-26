/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AnimationTrigger.h"
#include "Document.h"
#include "DocumentTimeline.h"
#include "StyleSingleAnimationRange.h"
#include "TriggerTimeline.h"

namespace WebCore {

Ref<AnimationTrigger> AnimationTrigger::create(Ref<AnimationTimeline>&& timeline)
{
    return adoptRef(*new AnimationTrigger(WTF::move(timeline), std::nullopt));
}

Ref<AnimationTrigger> AnimationTrigger::create(Document& document, AnimationTriggerOptions&& options)
{
    auto timeline = [&]() -> Ref<AnimationTimeline> {
        if (options.timeline && *options.timeline)
            return options.timeline->releaseNonNull();
        return document.timeline();
    }();

    return adoptRef(*new AnimationTrigger(WTF::move(timeline), options.behavior));
}

AnimationTrigger::AnimationTrigger(Ref<AnimationTimeline>&& timeline, std::optional<AnimationTriggerBehavior> behavior)
    : m_timeline(WTF::move(timeline))
    , m_activationRange({
        .start = Style::SingleAnimationRangeStart { CSS::Keyword::Normal { } },
        .end = Style::SingleAnimationRangeEnd { CSS::Keyword::Normal { } },
        .startZoom = Style::ZoomFactor::none(),
        .endZoom = Style::ZoomFactor::none(),
    })
{
    if (behavior)
        m_behavior = *behavior;
}

bool AnimationTrigger::isInActivationRange() const
{
    RefPtr triggerTimeline = dynamicDowncast<TriggerTimeline>(m_timeline);
    if (!triggerTimeline)
        return false;

    auto triggerTimelineTime = triggerTimeline->currentTime(UseCachedCurrentTime::No);
    if (!triggerTimelineTime)
        return false;

    auto activationInterval = triggerTimeline->intervalForAttachmentRange(m_activationRange);
    auto activationRangeStart = activationInterval.first;
    auto activationRangeEnd = activationInterval.second;

    TextStream ts;
    ts << "[GRAOUTS] Trigger activation range start = " << activationRangeStart << ", Trigger activation range end = " << activationRangeEnd << ", trigger timeline time = " << *triggerTimelineTime << ".";
    WTFLogAlways("%s", ts.release().ascii().data());

    return *triggerTimelineTime >= activationRangeStart && *triggerTimelineTime <= activationRangeEnd;
}

} // namespace WebCore
