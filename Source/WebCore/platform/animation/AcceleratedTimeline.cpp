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
#include "ScrollTimeline.h"

#if ENABLE(THREADED_ANIMATION_RESOLUTION)

#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(AcceleratedTimeline);

Ref<AcceleratedTimeline> AcceleratedTimeline::create(Seconds originTime)
{
    return adoptRef(*new AcceleratedTimeline(originTime));
}

Ref<AcceleratedTimeline> AcceleratedTimeline::create(const ScrollTimeline& source)
{
    // FIXME: process ViewTimeline as well.
    return adoptRef(*new AcceleratedTimeline(Type::Scroll, source.duration(), source.axis()));
}

Ref<AcceleratedTimeline> AcceleratedTimeline::create(Type type, WTF::UUID&& identifier, std::optional<WebAnimationTime>&& duration, std::optional<Seconds>&& originTime, std::optional<ScrollingNodeID>&& source, ScrollAxis axis)
{
    return adoptRef(*new AcceleratedTimeline(type, WTFMove(identifier), WTFMove(duration), WTFMove(originTime), WTFMove(source), axis));
}

AcceleratedTimeline::AcceleratedTimeline(Type type)
    : m_type(type)
    , m_identifier(WTF::UUID::createVersion4Weak())
{
}

AcceleratedTimeline::AcceleratedTimeline(Seconds originTime)
    : AcceleratedTimeline(Type::Document)
{
    m_originTime = originTime;
}

AcceleratedTimeline::AcceleratedTimeline(Type type, std::optional<WebAnimationTime> duration, ScrollAxis axis)
    : AcceleratedTimeline(type)
{
    m_duration = duration;
    m_axis = axis;
}

AcceleratedTimeline::AcceleratedTimeline(Type type, WTF::UUID&& identifier, std::optional<WebAnimationTime>&& duration, std::optional<Seconds>&& originTime, std::optional<ScrollingNodeID>&& source, ScrollAxis axis)
    : m_type(type)
    , m_identifier(WTFMove(identifier))
    , m_duration(WTFMove(duration))
    , m_originTime(WTFMove(originTime))
    , m_source(WTFMove(source))
    , m_axis(axis)
{
}

void AcceleratedTimeline::setProgress(std::optional<double> progress)
{
    ASSERT(isProgressBased());
    if (progress) {
        auto normalizedProgress = std::clamp(*progress, 0.0, 1.0);
        m_currentTime = WebAnimationTime::fromPercentage(normalizedProgress * 100);
    } else
        m_currentTime = std::nullopt;
}

void AcceleratedTimeline::setMonotonicTime(MonotonicTime now)
{
    ASSERT(m_originTime);
    ASSERT(isMonotonic());
    m_currentTime = now.secondsSinceEpoch() - *m_originTime;
}

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATION_RESOLUTION)
