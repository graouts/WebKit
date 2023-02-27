/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "AcceleratedTimelineScheduler.h"

#if ENABLE(THREADED_ANIMATION_RESOLUTION)

#include "EventDispatcher.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/AcceleratedTimeline.h>
#include <WebCore/DisplayRefreshMonitor.h>
#include <WebCore/Scrollbar.h>
#include <wtf/SystemTracing.h>

namespace WebKit {

AcceleratedTimelineScheduler::AcceleratedTimelineScheduler()
{
}

AcceleratedTimelineScheduler::~AcceleratedTimelineScheduler()
{
    stopAllDisplayLinks();
}

std::optional<AcceleratedTimelineScheduler::DisplayProperties> AcceleratedTimelineScheduler::displayProperties(WebCore::PageIdentifier pageId) const
{
    ASSERT(pageId);
    auto displayPropertiesIterator = m_displayProperties.find(pageId);
    if (displayPropertiesIterator == m_displayProperties.end())
        return std::nullopt;
    return { displayPropertiesIterator->value };
}

void AcceleratedTimelineScheduler::startDisplayLink(WebCore::PageIdentifier pageId)
{
    auto displayProperties = this->displayProperties(pageId);
    if (!displayProperties)
        return;

    auto displayId = displayProperties->displayId;
    auto iterator = m_observers.find(displayId);
    if (iterator != m_observers.end())
        return;

    LOG_WITH_STREAM(Animations, stream << "AcceleratedTimelineScheduler::startDisplayLink() for pageId " << pageId << " and displayId " << displayId);
    auto observerId = DisplayLinkObserverID::generate();
#if HAVE(CVDISPLAYLINK)
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPageProxy::StartAcceleratedAnimationDisplayLink(observerId, displayId), pageId);
#endif
    m_observers.set(displayId, WTFMove(observerId));
}

void AcceleratedTimelineScheduler::stopDisplayLink(WebCore::PageIdentifier pageId)
{
    auto displayProperties = this->displayProperties(pageId);
    if (!displayProperties)
        return;

    auto displayId = displayProperties->displayId;
    auto iterator = m_observers.find(displayId);
    if (iterator == m_observers.end())
        return;

    LOG_WITH_STREAM(Animations, stream << "AcceleratedTimelineScheduler::stopDisplayLink() for pageId " << pageId << " and displayId " << displayId);
#if HAVE(CVDISPLAYLINK)
    auto observerId = iterator->value;
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPageProxy::StopAcceleratedAnimationDisplayLink(observerId, displayId), pageId);
#endif
    m_observers.remove(iterator);
}

void AcceleratedTimelineScheduler::stopAllDisplayLinks()
{
#if HAVE(CVDISPLAYLINK)
    for (auto& [pageId, displayProperties] : m_displayProperties) {
        auto displayId = displayProperties.displayId;
        auto iterator = m_observers.find(displayId);
        if (iterator != m_observers.end()) {
            auto observerId = iterator->value;
            WebProcess::singleton().parentProcessConnection()->send(Messages::WebPageProxy::StopAcceleratedAnimationDisplayLink(observerId, displayId), pageId);
        }
    }
#endif
    m_observers.clear();
}

void AcceleratedTimelineScheduler::pageScreenDidChange(WebCore::PageIdentifier pageId, WebCore::PlatformDisplayID displayId, std::optional<unsigned> nominalFramesPerSecond)
{
    DisplayProperties properties;
    properties.displayId = displayId;
    properties.nominalFrameRate = nominalFramesPerSecond.value_or(WebCore::FullSpeedFramesPerSecond);
    m_displayProperties.set(pageId, WTFMove(properties));

    // FIXME: Should we stop and restart the display link there?
}

void AcceleratedTimelineScheduler::displayDidRefresh(WebCore::PlatformDisplayID displayId)
{
    for (auto& [pageId, displayProperties] : m_displayProperties) {
        if (displayProperties.displayId == displayId)
            updateTimelinesForPageId(pageId);
    }
}

void AcceleratedTimelineScheduler::startUpdatingPage(WebCore::PageIdentifier pageId)
{
    startDisplayLink(pageId);
}

void AcceleratedTimelineScheduler::stopUpdatingPage(WebCore::PageIdentifier pageId)
{
    stopDisplayLink(pageId);
}

void AcceleratedTimelineScheduler::updateTimelinesForPageId(WebCore::PageIdentifier pageId)
{
    WebCore::AcceleratedTimeline::updateTimelinesForPageId(pageId, [&](bool shouldKeepUpdating) {
        if (!shouldKeepUpdating)
            stopUpdatingPage(pageId);
    });
}

} // namespace WebKit

#endif // ENABLE(THREADED_ANIMATION_RESOLUTION)
