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

#include <WebCore/AcceleratedEffect.h>
#include <WebCore/WebAnimationTime.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit {

class RemoteAnimationEffect : public RefCounted<RemoteAnimationEffect> {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(RemoteAnimationEffect);
public:
    static Ref<RemoteAnimationEffect> create(const WebCore::AcceleratedEffect&);

    RemoteAnimationTimeline* timeline() const { return m_timeline.get(); }
    void setTimeline(RefPtr<RemoteAnimationTimeline>&& timeline) { m_timeline = WTFMove(timeline); }

private:
    RemoteAnimationEffect(const WebCore::AcceleratedEffect&);

    RefPtr<RemoteAnimationTimeline> m_timeline;
    AnimationEffectTiming m_timing;
    Vector<Keyframe> m_keyframes;
    WebAnimationType m_animationType { WebAnimationType::WebAnimation };
    CompositeOperation m_compositeOperation { CompositeOperation::Replace };
    RefPtr<TimingFunction> m_defaultKeyframeTimingFunction;
    OptionSet<AcceleratedEffectProperty> m_animatedProperties;
    OptionSet<AcceleratedEffectProperty> m_disallowedProperties;
    double m_playbackRate { 1 };

    bool m_paused { false };
    std::optional<WebAnimationTime> m_startTime;
    std::optional<WebAnimationTime> m_holdTime;
};

} // namespace WebKit

#endif // ENABLE(THREADED_ANIMATION_RESOLUTION)
