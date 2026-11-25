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

#pragma once

#include "ScopedName.h"
#include "ScopedTimelines.h"
#include "ScrollAxis.h"
#include "Styleable.h"

namespace WebCore {

namespace Style {
struct ViewTimelineInsetItem;
}

// A style-originated timeline is a timeline that is assigned to a CSS Animation
// via the `animation-timeline` property. These timelines may be created directly
// as a result of that property being set to a `scroll()` or `view()` value, but
// the `scroll-timeline-name` and `view-timeline-name` properties may be used on
// another element to define those elements as the scroll timeline's source or view
// timeline's subject. Additionally, the `timeline-scope` property can also be used
// on a shared ancestor to determine allow a `{scroll|view}-timeline-name` value to
// be used as an `animation-timeline` property in more complex hierarchies.
//
// https://drafts.csswg.org/css-animations-2/#animation-timeline
// https://drafts.csswg.org/scroll-animations-1/#scroll-timeline-name
// https://drafts.csswg.org/scroll-animations-1/#view-timeline-name
// https://drafts.csswg.org/scroll-animations-1/#timeline-scope

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleOriginatedTimelinesController);
class StyleOriginatedTimelinesController final : public ScopedTimelines {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(StyleOriginatedTimelinesController, StyleOriginatedTimelinesController);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(StyleOriginatedTimelinesController);
public:
    explicit StyleOriginatedTimelinesController() = default;
    ~StyleOriginatedTimelinesController() = default;

    void registerNamedScrollTimeline(const Style::ScopedName&, const Styleable&, ScrollAxis);
    void registerNamedViewTimeline(const Style::ScopedName&, const Styleable&, ScrollAxis, const Style::ViewTimelineInsetItem&, const Style::ZoomFactor&);

private:
    std::optional<Style::ScopedName> resolutionScopedNameForAnimation(const CSSAnimation&) const override;
};

} // namespace WebCore
