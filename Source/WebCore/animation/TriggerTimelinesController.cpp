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
#include "TriggerTimelinesController.h"

#include "CSSAnimation.h"
#include "Document.h"
#include "DocumentTimeline.h"
#include "ElementInlines.h"
#include "EventLoop.h"
#include "KeyframeEffect.h"
#include "LocalDOMWindow.h"
#include "Logging.h"
#include "Page.h"
#include "Settings.h"
#include "StyleableInlines.h"
#include "ViewTimeline.h"
#include "WebAnimation.h"
#include "WebAnimationTypes.h"
#include <JavaScriptCore/VM.h>
#include <wtf/HashSet.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(TriggerTimelinesController);

void TriggerTimelinesController::registerNamedTimeline(const AtomString& name, const Styleable& subject, ScrollAxis axis, const Style::ViewTimelineInsetItem& insets)
{
    LOG_WITH_STREAM(Animations, stream << "TriggerTimelinesController::registerNamedTimeline: " << name << " subject: " << subject);

    auto& timelines = this->timelines(name);

    auto existingTimelineIndex = timelines.findIf([&](auto& timeline) {
        ASSERT(is<ViewTimeline>(timeline));
        RefPtr viewTimeline = dynamicDowncast<ViewTimeline>(timeline);
        return viewTimeline->subjectStyleable() == subject;
    });

    auto hasExistingTimeline = existingTimelineIndex != notFound;

    if (hasExistingTimeline) {
        Ref existingViewTimeline = downcast<ViewTimeline>(timelines[existingTimelineIndex].get());
        existingViewTimeline->setAxis(axis);
        existingViewTimeline->setInsets({ insets, Style::ZoomFactor::none() });
    } else {
        auto newViewTimeline = ViewTimeline::create({ name }, axis, insets, Style::ZoomFactor::none());
        newViewTimeline->setSubject(subject);
        registerTimeline(timelines, WTF::move(newViewTimeline));
    }
}

std::optional<Style::ScopedName> TriggerTimelinesController::resolutionScopedNameForAnimation(const CSSAnimation& animation) const
{
    Ref protectedAnimation = animation;
    auto& trigger = protectedAnimation->backingStyleAnimation().trigger();
    ASSERT(!trigger.isNone());
    ASSERT(trigger.size());
    return Style::ScopedName { trigger[0].name.value };
}

void TriggerTimelinesController::scrollableAreaDidScroll(const ScrollableArea&)
{
    // FIXME: check whether we have a matching scrollable area for one of our managed
    // timelines and schedule an animation update if that is the case.
}

} // namespace WebCore
