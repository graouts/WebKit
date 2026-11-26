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

#pragma once

#include <WebCore/AnimationTimeline.h>
#include <WebCore/AnimationTriggerBehavior.h>
#include <WebCore/AnimationTriggerOptions.h>
#include <WebCore/ResolvableTimelineRange.h>
#include <wtf/Ref.h>

namespace WebCore {

class AnimationTrigger : public RefCounted<AnimationTrigger> {

public:
    static Ref<AnimationTrigger> create(Ref<AnimationTimeline>&&);
    static Ref<AnimationTrigger> create(Document&, AnimationTriggerOptions&&);

    void setTimeline(Ref<AnimationTimeline>&& timeline) { m_timeline = WTF::move(timeline); }
    AnimationTimeline& timeline() const { return m_timeline.get(); }

    void setBehavior(AnimationTriggerBehavior behavior) { m_behavior = behavior; }
    AnimationTriggerBehavior behavior() const { return m_behavior; }

    bool isInActivationRange() const;

private:
    explicit AnimationTrigger(Ref<AnimationTimeline>&&, std::optional<AnimationTriggerBehavior>);

    Ref<AnimationTimeline> m_timeline;
    AnimationTriggerBehavior m_behavior { AnimationTriggerBehavior::Once };
    ResolvableTimelineRange m_activationRange;
};

} // namespace WebCore
