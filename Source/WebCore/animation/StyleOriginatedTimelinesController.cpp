/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "StyleOriginatedTimelinesController.h"

#include "AnimationEventBase.h"
#include "CSSAnimation.h"
#include "CSSTransition.h"
#include "Document.h"
#include "DocumentTimeline.h"
#include "ElementInlines.h"
#include "EventLoop.h"
#include "KeyframeEffect.h"
#include "LocalDOMWindow.h"
#include "Logging.h"
#include "Page.h"
#include "ScrollTimeline.h"
#include "Settings.h"
#include "StyleableInlines.h"
#include "StyleScope.h"
#include "ViewTimeline.h"
#include "WebAnimation.h"
#include "WebAnimationTypes.h"
#include "WebAnimationUtilities.h"
#include <JavaScriptCore/VM.h>
#include <wtf/HashSet.h>
#include <wtf/text/TextStream.h>

#if ENABLE(THREADED_ANIMATIONS)
#include "AcceleratedEffectStackUpdater.h"
#endif

namespace WebCore {
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleOriginatedTimelinesController);

void StyleOriginatedTimelinesController::registerNamedScrollTimeline(const Style::ScopedName& scopedName, const Styleable& source, ScrollAxis axis)
{
    LOG_WITH_STREAM(Animations, stream << "StyleOriginatedTimelinesController::registerNamedScrollTimeline: " << scopedName.name << " source: " << source);

    auto& timelines = this->timelines(scopedName.name);

    auto existingTimelineIndex = timelines.findIf([&](auto& timeline) {
        return !is<ViewTimeline>(timeline) && timeline->sourceStyleable() == source;
    });

    if (existingTimelineIndex != notFound) {
        auto& existingScrollTimeline = timelines[existingTimelineIndex].get();
        existingScrollTimeline.setAxis(axis);
    } else {
        auto newScrollTimeline = ScrollTimeline::create(scopedName, axis);
        newScrollTimeline->setSource(source);
        registerTimeline(timelines, WTF::move(newScrollTimeline));
    }
}

void StyleOriginatedTimelinesController::registerNamedViewTimeline(const Style::ScopedName& scopedName, const Styleable& subject, ScrollAxis axis, const Style::ViewTimelineInsetItem& insets, const Style::ZoomFactor& usedZoomForLength)
{
    LOG_WITH_STREAM(Animations, stream << "StyleOriginatedTimelinesController::registerNamedViewTimeline: " << scopedName.name << " subject: " << subject);

    auto& timelines = this->timelines(scopedName.name);

    auto existingTimelineIndex = timelines.findIf([&](auto& timeline) {
        if (RefPtr viewTimeline = dynamicDowncast<ViewTimeline>(timeline))
            return viewTimeline->subjectStyleable() == subject;
        return false;
    });

    auto hasExistingTimeline = existingTimelineIndex != notFound;

    if (hasExistingTimeline) {
        Ref existingViewTimeline = downcast<ViewTimeline>(timelines[existingTimelineIndex].get());
        existingViewTimeline->setAxis(axis);
        existingViewTimeline->setInsets(ResolvableViewTimelineInsets { insets, usedZoomForLength });
    } else {
        auto newViewTimeline = ViewTimeline::create(scopedName, axis, insets, usedZoomForLength);
        newViewTimeline->setSubject(subject);
    }
}

std::optional<Style::ScopedName> StyleOriginatedTimelinesController::resolutionScopedNameForAnimation(const CSSAnimation& animation) const
{
    return protect(animation)->backingStyleAnimation().timeline().tryScopedName();
}

} // namespace WebCore
