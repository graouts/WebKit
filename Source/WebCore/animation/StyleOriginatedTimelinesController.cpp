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

// These accessors are called once per timeline registered under a given name every time an
// animation using that name is attached, so they must not manufacture weak references: handing
// back the element the caller is about to compare against is enough.

static const WeakStyleable& originatingStyleable(const Ref<ScrollTimeline>& timeline LIFETIME_BOUND)
{
    // The timeline outlives this call through the caller's reference, so downcast without taking
    // a local reference of our own: the returned reference is bound to the timeline, not to us.
    if (auto* viewTimeline = dynamicDowncast<ViewTimeline>(timeline.get()))
        return viewTimeline->subjectStyleable();
    return timeline->sourceStyleable();
}

static Element* NODELETE originatingElement(const Ref<ScrollTimeline>& timeline)
{
    return originatingStyleable(timeline).element().get();
}

static Element* NODELETE originatingElementIncludingTimelineScope(const Ref<ScrollTimeline>& timeline)
{
    if (auto* timelineScopeElement = timeline->timelineScopeDeclaredElement())
        return timelineScopeElement;
    return originatingElement(timeline);
}

static Element* NODELETE originatingElementExcludingTimelineScope(const Ref<ScrollTimeline>& timeline)
{
    return timeline->timelineScopeDeclaredElement() ? nullptr : originatingElement(timeline);
}

static bool scopesTimelineName(const Style::NameScope& scope, const Style::CustomIdent& name)
{
    switch (scope.type) {
    case Style::NameScope::Type::None:
        return false;
    case Style::NameScope::Type::All:
        return true;
    case Style::NameScope::Type::Ident:
        return scope.names.contains(name);
    }
    ASSERT_NOT_REACHED();
    return false;
}

RefPtr<Element> StyleOriginatedTimelinesController::nearestTimelineScopeElement(const Element& element, const Style::CustomIdent& name)
{
    // https://drafts.csswg.org/scroll-animations-1/#timeline-scope
    // A `timeline-scope` declaration extends the named timeline's scope across the declaring
    // element's subtree, so the declaration that applies to a given element is the one on its
    // nearest composed tree ancestor naming that timeline.
    if (m_timelineScopeEntries.isEmptyIgnoringNullReferences())
        return nullptr;

    for (RefPtr ancestor = element.parentElementInComposedTree(); ancestor; ancestor = ancestor->parentElementInComposedTree()) {
        auto it = m_timelineScopeEntries.find(*ancestor);
        if (it == m_timelineScopeEntries.end())
            continue;
        if (it->value.containsIf([&](auto& entry) { return scopesTimelineName(entry.scope, name); }))
            return ancestor;
    }
    return nullptr;
}

ScrollTimeline& StyleOriginatedTimelinesController::inactiveNamedTimeline(const AtomString& name)
{
    auto& timelines = timelinesForName(name);
    timelines.append(ScrollTimeline::createInactiveStyleOriginatedTimeline(name));
    return timelines.last();
}

ScrollTimeline* StyleOriginatedTimelinesController::determineTreeOrder(const Vector<Ref<ScrollTimeline>>& ancestorTimelines, const Styleable& styleable, const Element* timelineScopeElement)
{
    RefPtr element = styleable.element;
    while (element) {
        Vector<Ref<ScrollTimeline>> matchedTimelines;
        for (auto& timeline : ancestorTimelines) {
            if (element == originatingElementIncludingTimelineScope(timeline))
                matchedTimelines.append(timeline);
        }
        if (!matchedTimelines.isEmpty()) {
            if (timelineScopeElement == element.get()) {
                // Naming conflict due to timeline-scope, see if the element declares a non-deferred timeline.
                for (auto& matchedTimeline : matchedTimelines) {
                    if (element == originatingElement(matchedTimeline))
                        return matchedTimeline.unsafePtr();
                }
                // Prefer the nearest timeline in hierarchy.
                for (auto& matchedTimeline : matchedTimelines | std::views::reverse) {
                    if (styleable.element.isComposedTreeDescendantOf(*originatingElement(matchedTimeline)))
                        return matchedTimeline.unsafePtr();
                }
                // Otherwise return the last of the matching timelines per https://github.com/w3c/csswg-drafts/issues/12581.
                return matchedTimelines.last().unsafePtr();
            }
            ASSERT(matchedTimelines.size() <= 2);
            // Favor scroll timelines in case of conflict
            if (!is<ViewTimeline>(matchedTimelines.first()))
                return matchedTimelines.first().unsafePtr();
            return matchedTimelines.last().unsafePtr();
        }
        // Has blocking timeline scope element
        if (timelineScopeElement == element.get()) {
            ASSERT(!ancestorTimelines.isEmpty());
            return &inactiveNamedTimeline(ancestorTimelines.first()->name().name);
        }
        element = element->parentElementInComposedTree();
    }

    // If we get to this point, this means we haven't found a matching timeline that has
    // its originating element in the provided `timeline-scope` element's hierarchy. Since
    // timelines match globally, we must use global tree order..
    ASSERT(!timelineScopeElement);
    auto sortedTimelines = ancestorTimelines;
    std::ranges::stable_sort(sortedTimelines, [](auto& lhs, auto& rhs) {
        auto lhsStyleable = originatingStyleable(lhs).styleable();
        auto rhsStyleable = originatingStyleable(rhs).styleable();
        ASSERT(lhsStyleable);
        ASSERT(rhsStyleable);

        if (lhsStyleable == rhsStyleable) {
            ASSERT(is<ViewTimeline>(lhs) != is<ViewTimeline>(rhs));
            if (!is<ViewTimeline>(lhs))
                return false;
        }

        return compareStyleablePositionsInDocumentTreeOrder(*lhsStyleable, *rhsStyleable);
    });
    return sortedTimelines.first().unsafePtr();
}

static bool timelineIsInScopeForTarget(const Ref<ScrollTimeline>& timeline, Element& targetElement, Style::ScopeOrdinal animationTimelineNameScopeOrdinal)
{
    ASSERT(targetElement.isConnected());
    RefPtr timelineOriginatingElement { originatingElement(timeline) };
    ASSERT(timelineOriginatingElement);
    CheckedPtr scrollTimelineNameStyleScope = Style::Scope::forOrdinal(*timelineOriginatingElement, timeline->name().scopeOrdinal);
    ASSERT(scrollTimelineNameStyleScope);
    return Style::resolveTreeScopedReference(targetElement, { timeline->name().name, animationTimelineNameScopeOrdinal }, [&](const Style::Scope& scope, const Style::ScopedName&) {
        return scrollTimelineNameStyleScope == &scope;
    });
}

ScrollTimeline* StyleOriginatedTimelinesController::determineTimelineForElement(const Vector<Ref<ScrollTimeline>>& timelines, const Styleable& styleable, const Style::ScopedName& targetTimelineName, const Element* timelineScopeElement)
{
    // https://drafts.csswg.org/scroll-animations-1/#timeline-scoping
    // A named scroll progress timeline or view progress timeline is referenceable by:
    // 1. the name-declaring element itself
    // 2. that element’s descendants
    // If multiple elements have declared the same timeline name, the matching timeline is the one declared on the nearest element in tree order.
    // In case of a name conflict on the same element, names declared later in the naming property (scroll-timeline-name, view-timeline-name) take
    // precedence, and scroll progress timelines take precedence over view progress timelines.
    Ref targetElement { styleable.element };
    if (!targetElement->isConnected())
        return nullptr;

    auto isInScopeForTarget = [&](const Ref<ScrollTimeline>& timeline) {
        return timelineIsInScopeForTarget(timeline, targetElement.get(), targetTimelineName.scopeOrdinal);
    };

    // While timeline names match globally, matching a timeline referenceable from the target's own
    // hierarchy remains the common case, and only such a timeline can win the tree order walk
    // performed by determineTreeOrder(). We gather those by walking the target's ancestors and
    // asking the index which timelines each of them makes referenceable, so that attaching an
    // animation costs the depth of the target rather than the number of timelines in the document
    // sharing this name.
    Vector<Ref<ScrollTimeline>> timelinesInTargetHierarchy;
    for (RefPtr element = targetElement.ptr(); element; element = element->parentElementInComposedTree()) {
        auto it = m_timelinesByMatchingElement.find(*element);
        if (it == m_timelinesByMatchingElement.end())
            continue;
        for (auto& timeline : it->value) {
            if (timeline->name().name == targetTimelineName.name && isInScopeForTarget(timeline))
                timelinesInTargetHierarchy.append(timeline);
        }
    }
    if (!timelinesInTargetHierarchy.isEmpty())
        return determineTreeOrder(timelinesInTargetHierarchy, styleable, timelineScopeElement);

    // Nothing in the target's own hierarchy declares this name, so every timeline sharing it is a
    // candidate and determineTreeOrder() will pick between them using global document order.
    Vector<Ref<ScrollTimeline>> timelinesOutsideTargetHierarchy;
    for (auto& timeline : timelines) {
        if (originatingElementIncludingTimelineScope(timeline) && isInScopeForTarget(timeline))
            timelinesOutsideTargetHierarchy.append(timeline);
    }
    if (timelinesOutsideTargetHierarchy.isEmpty())
        return nullptr;
    return determineTreeOrder(timelinesOutsideTargetHierarchy, styleable, timelineScopeElement);
}

// A named timeline is referenceable from its declaring element's hierarchy, or from that of the
// element whose `timeline-scope` hoists it. Indexing timelines by that element lets us resolve a
// name by walking the target's ancestors instead of scanning every timeline sharing the name.

void StyleOriginatedTimelinesController::addTimelineToElementIndex(const Ref<ScrollTimeline>& timeline)
{
    RefPtr matchingElement = originatingElementIncludingTimelineScope(timeline);
    if (!matchingElement)
        return;

    auto& timelines = m_timelinesByMatchingElement.ensure(*matchingElement, [] {
        return Vector<Ref<ScrollTimeline>> { };
    }).iterator->value;
    ASSERT(!timelines.contains(timeline));
    timelines.append(timeline);
}

void StyleOriginatedTimelinesController::removeTimelineFromElementIndex(const Ref<ScrollTimeline>& timeline)
{
    RefPtr matchingElement = originatingElementIncludingTimelineScope(timeline);
    if (!matchingElement)
        return;

    auto it = m_timelinesByMatchingElement.find(*matchingElement);
    if (it == m_timelinesByMatchingElement.end())
        return;

    it->value.removeAll(timeline);
    if (it->value.isEmpty())
        m_timelinesByMatchingElement.remove(it);
}

void StyleOriginatedTimelinesController::setTimelineScopeElementForTimeline(const Ref<ScrollTimeline>& timeline, const Element* timelineScopeElement)
{
    // This changes which element makes the timeline referenceable, so it must go through here
    // rather than through ScrollTimeline directly, or the index would be left pointing at the
    // element the timeline used to be reachable from.
    removeTimelineFromElementIndex(timeline);
    if (timelineScopeElement)
        timeline->setTimelineScopeElement(*timelineScopeElement);
    else
        timeline->clearTimelineScopeDeclaredElement();
    addTimelineToElementIndex(timeline);
}

Vector<Ref<ScrollTimeline>>& StyleOriginatedTimelinesController::timelinesForName(const AtomString& name)
{
    return m_nameToTimelineMap.ensure(name, [] {
        return Vector<Ref<ScrollTimeline>> { };
    }).iterator->value;
}

void StyleOriginatedTimelinesController::updateTimelineForTimelineScope(const Ref<ScrollTimeline>& timeline, const AtomString& name)
{
    RefPtr timelineElement = originatingElementExcludingTimelineScope(timeline);
    if (!timelineElement)
        return;

    if (RefPtr timelineScopeElement = nearestTimelineScopeElement(*timelineElement, Style::CustomIdent { name }))
        timeline->setTimelineScopeElement(*timelineScopeElement);
}

void StyleOriginatedTimelinesController::registerNamedScrollTimeline(const Style::ScopedName& scopedName, const Styleable& source, ScrollAxis axis)
{
    LOG_WITH_STREAM(Animations, stream << "StyleOriginatedTimelinesController::registerNamedScrollTimeline: " << scopedName.name << " source: " << source);

    auto& timelines = timelinesForName(scopedName.name);

    auto existingTimelineIndex = timelines.findIf([&](auto& timeline) {
        return !is<ViewTimeline>(timeline) && timeline->sourceStyleable() == source;
    });

    if (existingTimelineIndex != notFound) {
        auto& existingScrollTimeline = timelines[existingTimelineIndex].get();
        existingScrollTimeline.setAxis(axis);
    } else {
        auto newScrollTimeline = ScrollTimeline::create(scopedName, axis);
        newScrollTimeline->setSource(source);
        updateTimelineForTimelineScope(newScrollTimeline, scopedName.name);
        addTimelineToElementIndex(newScrollTimeline);
        timelines.append(WTF::move(newScrollTimeline));
        m_timelineNamesPendingAnimationUpdate.add(scopedName.name);
    }
}

void StyleOriginatedTimelinesController::updateCSSAnimationsAssociatedWithNamedTimeline(const AtomString& name)
{
    auto it = m_nameToTimelineMap.find(name);
    if (it == m_nameToTimelineMap.end())
        return;

    // First, we need to gather all CSS Animations attached to existing timelines
    // with the specified name. We do this prior to updating animation-to-timeline
    // relationship because this could mutate the timeline's animations list.
    HashSet<Ref<CSSAnimation>> cssAnimationsWithMatchingTimelineName;
    for (auto& timeline : it->value) {
        for (auto& animation : timeline->relevantAnimations()) {
            if (RefPtr cssAnimation = dynamicDowncast<CSSAnimation>(animation.get())) {
                if (!cssAnimation->owningElement())
                    continue;
                if (auto timelineName = cssAnimation->backingStyleAnimation().timeline().tryScopedName()) {
                    if (timelineName->name == name)
                        cssAnimationsWithMatchingTimelineName.add(*cssAnimation);
                }
            }
        }
    }

    for (auto& cssAnimation : cssAnimationsWithMatchingTimelineName)
        cssAnimation->syncStyleOriginatedTimeline();
}

void StyleOriginatedTimelinesController::removePendingOperationsForCSSAnimation(const CSSAnimation& animation)
{
    m_cssAnimationsPendingAttachment.removeAllMatching([&] (const auto& pendingAnimation) {
        return pendingAnimation.ptr() == &animation;
    });
}

void StyleOriginatedTimelinesController::documentDidResolveStyle()
{
    // Registering a timeline may change which timeline the CSS Animations using its name resolve
    // to, but only the complete set of timelines registered during this style resolution can tell
    // us what those animations should end up attached to. Re-syncing them as each timeline comes in
    // would be quadratic in the number of timelines sharing a name, so we coalesce the updates by
    // name and perform them a single time, here.
    auto timelineNamesPendingAnimationUpdate = std::exchange(m_timelineNamesPendingAnimationUpdate, { });
    for (auto& name : timelineNamesPendingAnimationUpdate)
        updateCSSAnimationsAssociatedWithNamedTimeline(name);

    auto cssAnimationsPendingAttachment = std::exchange(m_cssAnimationsPendingAttachment, { });
    for (auto& cssAnimationPendingAttachment : cssAnimationsPendingAttachment) {
        if (cssAnimationPendingAttachment->owningElement())
            attachAnimation(cssAnimationPendingAttachment.get(), AllowsDeferral::No);
    }

    // Purge any inactive named timeline no longer attached to an animation. These are never in
    // m_timelinesByMatchingElement since they have no originating element to be indexed under.
    m_nameToTimelineMap.removeIf([](auto& keyValuePair) {
        auto& timelines = keyValuePair.value;
        timelines.removeAllMatching([](auto& timeline) {
            return timeline->isInactiveStyleOriginatedTimeline() && timeline->relevantAnimations().isEmpty();
        });
        return timelines.isEmpty();
    });

    assertElementIndexIsConsistent();

    m_removedTimelines.clear();
}

void StyleOriginatedTimelinesController::assertElementIndexIsConsistent() const
{
#if ASSERT_ENABLED
    // Every registered timeline with an originating element must be reachable from that element
    // through the index, and nothing else may be.
    for (auto& [name, timelines] : m_nameToTimelineMap) {
        for (auto& timeline : timelines) {
            RefPtr matchingElement = originatingElementIncludingTimelineScope(timeline);
            if (!matchingElement)
                continue;
            auto it = m_timelinesByMatchingElement.find(*matchingElement);
            ASSERT(it != m_timelinesByMatchingElement.end());
            ASSERT(it->value.contains(timeline));
        }
    }

    for (auto entry : m_timelinesByMatchingElement) {
        for (auto& timeline : entry.value) {
            ASSERT(originatingElementIncludingTimelineScope(timeline) == &entry.key);
            auto it = m_nameToTimelineMap.find(timeline->name().name);
            ASSERT(it != m_nameToTimelineMap.end());
            ASSERT(it->value.contains(timeline));
        }
    }
#endif
}

void StyleOriginatedTimelinesController::registerNamedViewTimeline(const Style::ScopedName& scopedName, const Styleable& subject, ScrollAxis axis, const Style::ViewTimelineInsetItem& insets, const Style::ZoomFactor& usedZoomForLength)
{
    LOG_WITH_STREAM(Animations, stream << "StyleOriginatedTimelinesController::registerNamedViewTimeline: " << scopedName.name << " subject: " << subject);

    auto& timelines = timelinesForName(scopedName.name);

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
        updateTimelineForTimelineScope(newViewTimeline, scopedName.name);
        addTimelineToElementIndex(newViewTimeline);
        timelines.append(WTF::move(newViewTimeline));
    }

    if (!hasExistingTimeline)
        m_timelineNamesPendingAnimationUpdate.add(scopedName.name);
}

void StyleOriginatedTimelinesController::unregisterNamedTimeline(const AtomString& name, const Styleable& styleable)
{
    LOG_WITH_STREAM(Animations, stream << "StyleOriginatedTimelinesController::unregisterNamedTimeline: " << name << " styleable: " << styleable);

    auto it = m_nameToTimelineMap.find(name);
    if (it == m_nameToTimelineMap.end())
        return;

    auto& timelines = it->value;

    auto i = timelines.findIf([&] (auto& entry) {
        return originatingStyleable(entry) == styleable;
    });

    if (i == notFound)
        return;

    Ref timeline = timelines.at(i);

    // Make sure to remove the named timeline from our name-to-timelines map first,
    // such that re-syncing any CSS Animation previously registered with it resolves
    // their `animation-timeline` properly.
    timelines.removeAt(i);
    removeTimelineFromElementIndex(timeline);

    // Ensure we iterate on a copy of this timeline's registered animations as calling
    // CSSAnimation::syncStyleOriginatedTimeline() may mutate it.
    for (Ref animation : copyToVector(timeline->relevantAnimations())) {
        if (RefPtr cssAnimation = dynamicDowncast<CSSAnimation>(animation)) {
            if (cssAnimation->owningElement())
                cssAnimation->syncStyleOriginatedTimeline();
        }
    }

    if (timelines.isEmpty())
        m_nameToTimelineMap.remove(it);
    else
        m_timelineNamesPendingAnimationUpdate.add(name);
}

void StyleOriginatedTimelinesController::attachAnimation(CSSAnimation& animation)
{
    attachAnimation(animation, AllowsDeferral::Yes);
}

void StyleOriginatedTimelinesController::attachAnimation(CSSAnimation& animation, AllowsDeferral allowsDeferral)
{
    Ref protectedAnimation { animation };

    auto target = protectedAnimation->owningElement();
    if (!target)
        return;

    auto timelineName = protectedAnimation->backingStyleAnimation().timeline().tryScopedName();
    if (!timelineName)
        return;

    LOG_WITH_STREAM(Animations, stream << "StyleOriginatedTimelinesController::attachAnimation: " << timelineName->name << " target: " << *target);

    RefPtr relevantTimelineScopeElement = nearestTimelineScopeElement(target->element, Style::CustomIdent { timelineName->name });

    auto it = m_nameToTimelineMap.find(timelineName->name);
    auto hasNamedTimeline = it != m_nameToTimelineMap.end() && it->value.containsIf([&](auto& timeline) {
        auto* timelineScope = timeline->timelineScopeDeclaredElement();
        if (timelineScope && timelineScope != relevantTimelineScopeElement.get())
            return false;
        return !timeline->isInactiveStyleOriginatedTimeline();
    });

    // If we don't have an active named timeline yet and deferral is allowed,
    // just register a pending timeline attachment operation so we can try again
    // when style has resolved.
    if (!hasNamedTimeline && allowsDeferral == AllowsDeferral::Yes) {
        m_cssAnimationsPendingAttachment.append(animation);
        return;
    }

    if (!hasNamedTimeline) {
        ASSERT(allowsDeferral == AllowsDeferral::No);
        protectedAnimation->setTimeline(&inactiveNamedTimeline(timelineName->name));
    } else {
        auto& timelines = it->value;
        RefPtr timeline = determineTimelineForElement(timelines, *target, *timelineName, relevantTimelineScopeElement.get());
        LOG_WITH_STREAM(Animations, stream << "StyleOriginatedTimelinesController::attachAnimation: " << timelineName->name << " styleable: " << *target << " attaching to timeline of element: " << originatingStyleable(*timeline));
        // A deferred inactive timeline means there was a conflict with multiple timelines existing within
        // a parent element with a "timeline-scope" property. In that case, we must reconsider timeline attachment
        // once style resolution completes as further updates may occur that would yield a different timeline
        // and possibly also mark that animation's target as dirty to update the animated style.
        if (allowsDeferral == AllowsDeferral::Yes && timeline && timeline->isInactiveStyleOriginatedTimeline())
            m_cssAnimationsPendingAttachment.append(animation);
        protectedAnimation->setTimeline(WTF::move(timeline));
    }

    // Since we have no timeline defined for this name yet, we need
    // to mark this animation as pending attachment in case this name
    // resolves in the future.
    if (!protectedAnimation->timeline())
        m_cssAnimationsPendingAttachment.append(animation);
}

void StyleOriginatedTimelinesController::updateTimelinesForTimelineScope(Vector<Ref<ScrollTimeline>> entries, const Styleable& styleable)
{
    for (auto& entry : entries) {
        if (RefPtr entryElement = originatingElementExcludingTimelineScope(entry)) {
            Ref element { styleable.element };
            if (entryElement->isComposedTreeDescendantOf(element)) {
                setTimelineScopeElementForTimeline(entry, element.ptr());
                for (Ref animation : copyToVector(entry->relevantAnimations())) {
                    if (RefPtr cssAnimation = dynamicDowncast<CSSAnimation>(animation))
                        attachAnimation(*cssAnimation, AllowsDeferral::Yes);
                }
            }
        }
    }
}

void StyleOriginatedTimelinesController::setTimelineScopeEntry(const Style::NameScope& scope, const Styleable& styleable)
{
    // An element's style may be resolved any number of times while it declares the same
    // `timeline-scope` value, so only record an entry we don't have already.
    // FIXME: when an element's `timeline-scope` names change, the entry for the previous names
    // is left behind, and timelines matching those names remain scoped to this element.
    auto& entries = m_timelineScopeEntries.ensure(styleable.element, [] {
        return Vector<TimelineScopeEntry> { };
    }).iterator->value;

    auto hasMatchingEntry = entries.containsIf([&](auto& entry) {
        return entry.pseudoElementIdentifier == styleable.pseudoElementIdentifier && entry.scope == scope;
    });
    if (!hasMatchingEntry)
        entries.append(TimelineScopeEntry { scope, styleable.pseudoElementIdentifier });
}

void StyleOriginatedTimelinesController::removeTimelineScopeEntry(const Styleable& styleable)
{
    auto it = m_timelineScopeEntries.find(styleable.element);
    if (it == m_timelineScopeEntries.end())
        return;

    it->value.removeAllMatching([&](auto& entry) {
        return entry.pseudoElementIdentifier == styleable.pseudoElementIdentifier;
    });
    if (it->value.isEmpty())
        m_timelineScopeEntries.remove(it);
}

void StyleOriginatedTimelinesController::updateNamedTimelineMapForTimelineScope(const Style::NameScope& scope, const Styleable& styleable)
{
    LOG_WITH_STREAM(Animations, stream << "StyleOriginatedTimelinesController::updateNamedTimelineMapForTimelineScope: " << scope << " styleable: " << styleable);

    // https://drafts.csswg.org/scroll-animations-1/#timeline-scope
    // This property declares the scope of the specified timeline names to extend across this element’s subtree. This allows a named timeline
    // (such as a named scroll progress timeline or named view progress timeline) to be referenced by elements outside the timeline-defining element’s
    // subtree—​for example, by siblings, cousins, or ancestors.
    switch (scope.type) {
    case Style::NameScope::Type::None: {
        HashSet<Ref<ScrollTimeline>> namedTimelinesToUpdate;
        for (auto& entry : m_nameToTimelineMap) {
            for (auto& timeline : entry.value) {
                if (timeline->timelineScopeDeclaredElement() == &styleable.element)
                    setTimelineScopeElementForTimeline(timeline, nullptr);
                // Make sure to track this timeline to be updated in a separate
                // step since updating timeline relationships could affect m_nameToTimelineMap.
                namedTimelinesToUpdate.add(timeline.get());
            }
        }
        removeTimelineScopeEntry(styleable);
        for (auto& timeline : namedTimelinesToUpdate) {
            for (Ref animation : copyToVector(timeline->relevantAnimations())) {
                if (RefPtr cssAnimation = dynamicDowncast<CSSAnimation>(animation)) {
                    if (cssAnimation->owningElement())
                        cssAnimation->syncStyleOriginatedTimeline();
                }
            }
        }
        break;
    }
    case Style::NameScope::Type::All:
        for (auto& entry : m_nameToTimelineMap)
            updateTimelinesForTimelineScope(entry.value, styleable);
        setTimelineScopeEntry(scope, styleable);
        break;
    case Style::NameScope::Type::Ident:
        for (auto& name : scope.names) {
            auto it = m_nameToTimelineMap.find(name.value);
            if (it != m_nameToTimelineMap.end())
                updateTimelinesForTimelineScope(it->value, styleable);
        }
        setTimelineScopeEntry(scope, styleable);
        break;
    }
}

bool StyleOriginatedTimelinesController::isPendingTimelineAttachment(const WebAnimation& animation) const
{
    if (RefPtr cssAnimation = dynamicDowncast<CSSAnimation>(animation)) {
        return m_cssAnimationsPendingAttachment.containsIf([&](auto& pendingAnimation) {
            return pendingAnimation.ptr() == cssAnimation.get();
        });
    }
    return false;
}

void StyleOriginatedTimelinesController::unregisterNamedTimelinesAssociatedWithElement(const Styleable& styleable)
{
    LOG_WITH_STREAM(Animations, stream << "StyleOriginatedTimelinesController::unregisterNamedTimelinesAssociatedWithElement element: " << styleable);

    m_nameToTimelineMap.removeIf([&](auto& entry) {
        auto& timelines = entry.value;
        for (size_t i = 0; i < timelines.size(); ++i) {
            auto& timeline = timelines[i];
            if (originatingStyleable(timeline) == styleable) {
                m_removedTimelines.add(timeline.get());
                removeTimelineFromElementIndex(timeline);
                timelines.removeAt(i--);
            }
        }
        return timelines.isEmpty();
    });
}

void StyleOriginatedTimelinesController::styleableWasRemoved(const Styleable& styleable)
{
    // A removed element no longer scopes any timeline name.
    removeTimelineScopeEntry(styleable);

    for (Ref timeline : m_removedTimelines) {
        if (originatingStyleable(timeline) != styleable)
            continue;
        for (Ref animation : copyToVector(timeline->relevantAnimations())) {
            if (RefPtr cssAnimation = dynamicDowncast<CSSAnimation>(animation.get())) {
                if (auto owningElement = cssAnimation->owningElement()) {
                    attachAnimation(*cssAnimation, AllowsDeferral::Yes);
                    Ref { owningElement->element }->invalidateStyleForAnimation();
                }
            }
        }
    }
}

} // namespace WebCore

