/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(THREADED_ANIMATION_RESOLUTION)

#include "ScrollAxis.h"
#include "ScrollingNodeID.h"
#include "WebAnimationTime.h"
#include <wtf/Seconds.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class ScrollTimeline;

class AcceleratedTimeline : public RefCounted<AcceleratedTimeline> {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(AcceleratedTimeline);
public:
    static Ref<AcceleratedTimeline> create(Seconds originTime);
    static Ref<AcceleratedTimeline> create(const ScrollTimeline&);

    bool isMonotonic() const { return !m_duration; }
    bool isProgressBased() const { return !isMonotonic(); }

    void setSource(std::optional<ScrollingNodeID> source) { m_source = source; }

    std::optional<WebAnimationTime> currentTime() const { return m_currentTime; }
    WEBCORE_EXPORT void setProgress(std::optional<double>);
    WEBCORE_EXPORT void setMonotonicTime(MonotonicTime);

    enum class Type : uint8_t { Document, Scroll, View };

    // Encoding support.
    WEBCORE_EXPORT static Ref<AcceleratedTimeline> create(Type, std::optional<WebAnimationTime>&& duration, std::optional<Seconds>&& originTime, std::optional<ScrollingNodeID>&&, ScrollAxis);
    Type type() const { return m_type; }
    const std::optional<WebAnimationTime> duration() const { return m_duration; }
    const std::optional<Seconds> originTime() const { return m_originTime; }
    const std::optional<ScrollingNodeID> source() const { return m_source; }
    ScrollAxis axis() const { return m_axis; }

    virtual ~AcceleratedTimeline() = default;

private:
    AcceleratedTimeline(Type);
    AcceleratedTimeline(Seconds originTime);
    AcceleratedTimeline(Type, std::optional<WebAnimationTime> duration, ScrollAxis);
    AcceleratedTimeline(Type, std::optional<WebAnimationTime>&& duration, std::optional<Seconds>&& originTime, std::optional<ScrollingNodeID>&&, ScrollAxis);

    Type m_type;
    std::optional<WebAnimationTime> m_duration;
    std::optional<Seconds> m_originTime;
    std::optional<ScrollingNodeID> m_source;
    ScrollAxis m_axis { ScrollAxis::Block };

    std::optional<WebAnimationTime> m_currentTime;
};

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATION_RESOLUTION)
