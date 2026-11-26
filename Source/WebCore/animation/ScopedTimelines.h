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

#include "CSSAnimation.h"
#include "ScopedName.h"
#include "StyleNameScope.h"
#include "Styleable.h"
#include <wtf/ListHashSet.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

class ScrollTimeline;
class WebAnimation;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleOriginatedTimelinesController);
class ScopedTimelines : public CanMakeCheckedPtr<ScopedTimelines> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(ScopedTimelines, ScopedTimelines);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(ScopedTimelines);
public:
    explicit ScopedTimelines() = default;
    ~ScopedTimelines() = default;

    void registerNamedTimeline(const Ref<ScrollTimeline>&, const AtomString&);
    void unregisterNamedTimeline(const AtomString&, const Styleable&);
    void resolveScopedTimelineForAnimation(CSSAnimation&);
    void updateNamedTimelineMapForTimelineScope(const Style::NameScope&, const Styleable&);
    void updateTimelineForTimelineScope(const Ref<ScrollTimeline>&, const AtomString&);
    void unregisterNamedTimelinesAssociatedWithElement(const Styleable&);
    void removePendingOperationsForCSSAnimation(const CSSAnimation&);
    bool isPendingTimelineAttachment(const WebAnimation&) const;
    void documentDidResolveStyle();
    void styleableWasRemoved(const Styleable&);

protected:
    virtual std::optional<Style::ScopedName> resolutionScopedNameForAnimation(const CSSAnimation&) const { return std::nullopt; }

    Vector<Ref<ScrollTimeline>>& timelines(const AtomString&) LIFETIME_BOUND;
    void registerTimeline(Vector<Ref<ScrollTimeline>>&, Ref<ScrollTimeline>&&);
    void forEachTimeline(NOESCAPE const Function<bool(ScrollTimeline&)>&) const;
private:
    Vector<WeakStyleable> scopeElements(const Style::CustomIdent&);
    void updateCSSAnimationsAssociatedWithNamedTimeline(const AtomString&);
    void updateTimelinesForScope(Vector<Ref<ScrollTimeline>>, const Styleable&);

    enum class AllowsDeferral : bool { No, Yes };
    void resolveScopedTimelineForAnimation(CSSAnimation&, AllowsDeferral);
    RefPtr<ScrollTimeline> timelineForElement(const Vector<Ref<ScrollTimeline>>&, const Styleable&, Style::ScopeOrdinal targetTimelineScopeOrdinal, const Element*);
    RefPtr<ScrollTimeline> scopedTimelineForElement(const Vector<Ref<ScrollTimeline>>&, const Styleable&, const Element*);
    ScrollTimeline& inactiveTimeline(const AtomString&);

    using ScopeEntry = std::pair<Style::NameScope, WeakStyleable>;
    ListHashSet<Ref<CSSAnimation>> m_cssAnimationsPendingAttachment;
    Vector<ScopeEntry> m_scopeEntries;
    HashMap<AtomString, Vector<Ref<ScrollTimeline>>> m_nameToTimelineMap;
    HashSet<Ref<ScrollTimeline>> m_removedTimelines;
    HashSet<AtomString> m_timelineNamesPendingAnimationUpdate;
};

} // namespace WebCore
