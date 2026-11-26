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
#include "TriggerTimeline.h"

#include "WebAnimation.h"

namespace WebCore {

Ref<TriggerTimeline> TriggerTimeline::create(const Style::ScopedName& scopedName, ScrollAxis axis, const Style::ViewTimelineInsetItem& insets, const Style::ZoomFactor& usedZoomForLength)
{
    return adoptRef(*new TriggerTimeline(scopedName, axis, insets, usedZoomForLength));
}

TriggerTimeline::TriggerTimeline(const Style::ScopedName& scopedName, ScrollAxis axis, const Style::ViewTimelineInsetItem& insets, const Style::ZoomFactor& usedZoomForLength)
    : ViewTimeline(scopedName, axis, insets, usedZoomForLength)
{
}

void TriggerTimeline::setAnimation(WebAnimation* animation)
{
    m_animation = animation;
}

WebAnimation* TriggerTimeline::animation() const
{
    return m_animation.get();
}

} // namespace WebCore
