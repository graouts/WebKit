/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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
#include "AnimationEffect.h"

#include "CSSAnimation.h"
#include "CSSNumericFactory.h"
#include "CSSNumericValue.h"
#include "CSSPropertyParserConsumer+TimingFunction.h"
#include "CSSTimingFunctionValue.h"
#include "CommonAtomStrings.h"
#include "FillMode.h"
#include "JSComputedEffectTiming.h"
#include "ScriptExecutionContext.h"
#include "WebAnimation.h"
#include "WebAnimationUtilities.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(AnimationEffect);

AnimationEffect::AnimationEffect() = default;

AnimationEffect::~AnimationEffect() = default;

void AnimationEffect::setAnimation(WebAnimation* animation)
{
    if (m_animation == animation)
        return;

    m_animation = animation;
    if (animation)
        animation->updateRelevance();
}

enum class IsComputed : bool { No, Yes };
static std::variant<double, RefPtr<CSSNumericValue>, String> durationAPIValue(const WebAnimationTime& duration, IsComputed isComputed)
{
    if (auto percent = duration.percentage()) {
        if (*percent == 100)
            return autoAtom();
        return CSSNumericFactory::percent(*percent);
    }

    ASSERT(duration.time());
    if (duration.isZero()) {
        if (isComputed == IsComputed::Yes)
            return 0.0;
        return autoAtom();
    }

    CSSNumberish numberishDuration { duration };
    if (auto* doubleValue = std::get_if<double>(&numberishDuration))
        return *doubleValue;

    ASSERT(std::holds_alternative<RefPtr<CSSNumericValue>>(numberishDuration));
    return std::get<RefPtr<CSSNumericValue>>(numberishDuration);
}

EffectTiming AnimationEffect::getBindingsTiming() const
{
    if (auto* styleOriginatedAnimation = dynamicDowncast<StyleOriginatedAnimation>(animation()))
        styleOriginatedAnimation->flushPendingStyleChanges();

    EffectTiming timing;
    timing.delay = secondsToWebAnimationsAPITime(m_timing.specifiedStartDelay);
    timing.endDelay = secondsToWebAnimationsAPITime(m_timing.specifiedEndDelay);
    timing.fill = m_timing.fill;
    timing.iterationStart = m_timing.iterationStart;
    timing.iterations = m_timing.iterations;
    timing.duration = durationAPIValue(m_timing.specifiedIterationDuration, IsComputed::No);
    timing.direction = m_timing.direction;
    timing.easing = m_timing.timingFunction->cssText();
    return timing;
}

AnimationEffectTiming::ResolutionData AnimationEffect::resolutionData(std::optional<WebAnimationTime> startTime) const
{
    if (!m_animation)
        return { };

    RefPtr animation = m_animation.get();
    RefPtr timeline = animation->timeline();
    return {
        timeline ? timeline->currentTime() : std::nullopt,
        timeline ? timeline->duration() : std::nullopt,
        startTime ? startTime : animation->startTime(),
        animation->currentTime(startTime),
        animation->playbackRate()
    };
}

BasicEffectTiming AnimationEffect::getBasicTiming(std::optional<WebAnimationTime> startTime) const
{
    return m_timing.getBasicTiming(resolutionData(startTime));
}

ComputedEffectTiming AnimationEffect::getBindingsComputedTiming() const
{
    if (auto* styleOriginatedAnimation = dynamicDowncast<StyleOriginatedAnimation>(animation()))
        styleOriginatedAnimation->flushPendingStyleChanges();
    return getComputedTiming();
}

ComputedEffectTiming AnimationEffect::getComputedTiming(std::optional<WebAnimationTime> startTime) const
{
    auto data = resolutionData(startTime);
    auto resolvedTiming = m_timing.resolve(data);

    ComputedEffectTiming computedTiming;
    computedTiming.delay = secondsToWebAnimationsAPITime(m_timing.specifiedStartDelay);
    computedTiming.endDelay = secondsToWebAnimationsAPITime(m_timing.specifiedEndDelay);
    computedTiming.fill = m_timing.fill == FillMode::Auto ? FillMode::None : m_timing.fill;
    computedTiming.iterationStart = m_timing.iterationStart;
    computedTiming.iterations = m_timing.iterations;
    computedTiming.duration = durationAPIValue(m_timing.intrinsicIterationDuration, IsComputed::Yes);
    computedTiming.direction = m_timing.direction;
    computedTiming.easing = m_timing.timingFunction->cssText();
    computedTiming.endTime = m_timing.endTime;
    computedTiming.activeDuration = m_timing.activeDuration;
    computedTiming.localTime = data.localTime;
    computedTiming.simpleIterationProgress = resolvedTiming.simpleIterationProgress;
    computedTiming.progress = resolvedTiming.transformedProgress;
    computedTiming.currentIteration = resolvedTiming.currentIteration;
    computedTiming.phase = resolvedTiming.phase;
    computedTiming.before = resolvedTiming.before;
    return computedTiming;
}

ExceptionOr<void> AnimationEffect::bindingsUpdateTiming(Document& document, std::optional<OptionalEffectTiming> timing)
{
    auto retVal = updateTiming(document, timing);
    if (!retVal.hasException() && timing) {
        if (auto* cssAnimation = dynamicDowncast<CSSAnimation>(animation()))
            cssAnimation->effectTimingWasUpdatedUsingBindings(*timing);
    }
    return retVal;
}

ExceptionOr<void> AnimationEffect::updateTiming(Document& document, std::optional<OptionalEffectTiming> timing)
{
    // 6.5.4. Updating the timing of an AnimationEffect
    // https://drafts.csswg.org/web-animations/#updating-animationeffect-timing

    // To update the timing properties of an animation effect, effect, from an EffectTiming or OptionalEffectTiming object, input, perform the following steps:
    if (!timing)
        return { };

    // 1. If the iterationStart member of input is present and less than zero, throw a TypeError and abort this procedure.
    if (timing->iterationStart) {
        if (timing->iterationStart.value() < 0)
            return Exception { ExceptionCode::TypeError };
    }

    // 2. If the iterations member of input is present, and less than zero or is the value NaN, throw a TypeError and abort this procedure.
    if (timing->iterations) {
        if (timing->iterations.value() < 0 || std::isnan(timing->iterations.value()))
            return Exception { ExceptionCode::TypeError };
    }

    // 3. If the duration member of input is present, and less than zero or is the value NaN, throw a TypeError and abort this procedure.
    // FIXME: should it not throw an exception on a string other than "auto"?
    if (timing->duration) {
        if (std::holds_alternative<double>(timing->duration.value())) {
            auto durationAsDouble = std::get<double>(timing->duration.value());
            if (durationAsDouble < 0 || std::isnan(durationAsDouble))
                return Exception { ExceptionCode::TypeError };
        } else {
            if (std::get<String>(timing->duration.value()) != autoAtom())
                return Exception { ExceptionCode::TypeError };
        }
    }

    // 4. If the easing member of input is present but cannot be parsed using the <timing-function> production [CSS-EASING-1], throw a TypeError and abort this procedure.
    if (!timing->easing.isNull()) {
        CSSParserContext parsingContext(document);
        auto timingFunctionResult = CSSPropertyParserHelpers::parseTimingFunction(timing->easing, parsingContext);
        if (!timingFunctionResult)
            return Exception { ExceptionCode::TypeError };
        m_timing.timingFunction = WTFMove(timingFunctionResult);
    }

    // 5. Assign each member present in input to the corresponding timing property of effect as follows:
    //
    //    delay → start delay
    //    endDelay → end delay
    //    fill → fill mode
    //    iterationStart → iteration start
    //    iterations → iteration count
    //    duration → iteration duration
    //    direction → playback direction
    //    easing → timing function

    auto needsToNormalizeSpecifiedTiming = false;

    if (timing->delay) {
        m_timing.specifiedStartDelay = Seconds::fromMilliseconds(*timing->delay);
        needsToNormalizeSpecifiedTiming = true;
    }

    if (timing->endDelay) {
        m_timing.specifiedEndDelay = Seconds::fromMilliseconds(*timing->endDelay);
        needsToNormalizeSpecifiedTiming = true;
    }

    if (timing->fill)
        m_timing.fill = timing->fill.value();

    if (timing->iterationStart)
        m_timing.iterationStart = timing->iterationStart.value();

    if (timing->iterations) {
        m_timing.iterations = timing->iterations.value();
        needsToNormalizeSpecifiedTiming = true;
    }

    if (auto duration = timing->duration) {
        auto* durationDouble = std::get_if<double>(&*duration);
        m_hasAutoDuration = !durationDouble;
        m_timing.specifiedIterationDuration = durationDouble ? Seconds::fromMilliseconds(*durationDouble) : 0_s;
        needsToNormalizeSpecifiedTiming = true;
    }

    if (timing->direction)
        m_timing.direction = timing->direction.value();

    if (needsToNormalizeSpecifiedTiming)
        normalizeSpecifiedTiming();
    updateStaticTimingProperties();

    if (m_animation)
        m_animation->effectTimingDidChange();

    return { };
}

// Move this to AnimationEffectTiming and pass in the timeline duration and whether the duration is auto.
void AnimationEffect::normalizeSpecifiedTiming()
{
    auto durationIsAuto = m_hasAutoDuration ? AnimationEffectTiming::DurationIsAuto::Yes : AnimationEffectTiming::DurationIsAuto::No;
    auto timelineDuration = [&] -> std::optional<WebAnimationTime> {
        if (m_animation) {
            if (RefPtr timeline = m_animation->timeline())
                return timeline->duration();
        }
        return std::nullopt;
    }();
    m_timing.normalizeSpecifiedTiming(timelineDuration, durationIsAuto);
}

void AnimationEffect::updateStaticTimingProperties()
{
    m_timing.updateComputedProperties([&] {
        if (m_animation)
            return m_animation->playbackRate();
        return 1.0;
    }());
}

ExceptionOr<void> AnimationEffect::setIterationStart(double iterationStart)
{
    // https://drafts.csswg.org/web-animations-1/#dom-animationeffecttiming-iterationstart
    // If an attempt is made to set this attribute to a value less than zero, a TypeError must
    // be thrown and the value of the iterationStart attribute left unchanged.
    if (iterationStart < 0)
        return Exception { ExceptionCode::TypeError };

    if (m_timing.iterationStart == iterationStart)
        return { };

    m_timing.iterationStart = iterationStart;

    return { };
}

ExceptionOr<void> AnimationEffect::setIterations(double iterations)
{
    // https://drafts.csswg.org/web-animations-1/#dom-animationeffecttiming-iterations
    // If an attempt is made to set this attribute to a value less than zero or a NaN value, a
    // TypeError must be thrown and the value of the iterations attribute left unchanged.
    if (iterations < 0 || std::isnan(iterations))
        return Exception { ExceptionCode::TypeError };

    if (m_timing.iterations == iterations)
        return { };
        
    m_timing.iterations = iterations;

    return { };
}

void AnimationEffect::setDelay(const Seconds& delay)
{
    if (m_timing.specifiedStartDelay == delay)
        return;

    m_timing.specifiedStartDelay = delay;
}

void AnimationEffect::setEndDelay(const Seconds& endDelay)
{
    if (m_timing.specifiedEndDelay == endDelay)
        return;

    m_timing.specifiedEndDelay = endDelay;
}

void AnimationEffect::setFill(FillMode fill)
{
    if (m_timing.fill == fill)
        return;

    m_timing.fill = fill;
}

void AnimationEffect::setIterationDuration(const Seconds& duration)
{
    if (m_timing.specifiedIterationDuration == duration)
        return;

    m_timing.specifiedIterationDuration = duration;
}

void AnimationEffect::setDirection(PlaybackDirection direction)
{
    if (m_timing.direction == direction)
        return;

    m_timing.direction = direction;
}

void AnimationEffect::setTimingFunction(const RefPtr<TimingFunction>& timingFunction)
{
    m_timing.timingFunction = timingFunction;
}

std::optional<double> AnimationEffect::progressUntilNextStep(double iterationProgress) const
{
    RefPtr stepsTimingFunction = dynamicDowncast<StepsTimingFunction>(m_timing.timingFunction);
    if (!stepsTimingFunction)
        return std::nullopt;

    auto numberOfSteps = stepsTimingFunction->numberOfSteps();
    auto nextStepProgress = ceil(iterationProgress * numberOfSteps) / numberOfSteps;
    return nextStepProgress - iterationProgress;
}

Seconds AnimationEffect::timeToNextTick(const BasicEffectTiming& timing) const
{
    switch (timing.phase) {
    case AnimationEffectPhase::Before:
        // The effect is in its "before" phase, in this case we can wait until it enters its "active" phase.
        return delay() - *timing.localTime;
    case AnimationEffectPhase::Active: {
        if (!ticksContinuouslyWhileActive())
            return endTime() - *timing.localTime;
        if (auto iterationProgress = getComputedTiming().simpleIterationProgress) {
            // In case we're in a range that uses a steps() timing function, we can compute the time until the next step starts.
            if (auto progressUntilNextStep = this->progressUntilNextStep(*iterationProgress))
                return iterationDuration() * *progressUntilNextStep;
        }
        // Other effects that continuously tick in the "active" phase will need to update their animated
        // progress at the immediate next opportunity.
        return 0_s;
    }
    case AnimationEffectPhase::After:
        // The effect is in its after phase, which means it will no longer update its progress, so it doens't need a tick.
        return Seconds::infinity();
    case AnimationEffectPhase::Idle:
        ASSERT_NOT_REACHED();
        return Seconds::infinity();
    }

    ASSERT_NOT_REACHED();
    return Seconds::infinity();
}

void AnimationEffect::animationTimelineDidChange(const AnimationTimeline*)
{
    normalizeSpecifiedTiming();
    updateStaticTimingProperties();
}

void AnimationEffect::animationPlaybackRateDidChange()
{
    updateStaticTimingProperties();
}

} // namespace WebCore
