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

#pragma once

#if ENABLE(THREADED_ANIMATION_RESOLUTION)

#include "DisplayLinkObserverID.h"
#include <WebCore/PageIdentifier.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Noncopyable.h>

namespace WebCore {
struct DisplayUpdate;
using FramesPerSecond = unsigned;
using PlatformDisplayID = uint32_t;
}

namespace WebKit {

class EventDispatcher;

class AcceleratedTimelineScheduler {
    WTF_MAKE_NONCOPYABLE(AcceleratedTimelineScheduler);
    WTF_MAKE_FAST_ALLOCATED;
public:
    AcceleratedTimelineScheduler();
    ~AcceleratedTimelineScheduler();

    void startUpdatingPage(WebCore::PageIdentifier);
    void stopUpdatingPage(WebCore::PageIdentifier);

    void displayDidRefresh(WebCore::PlatformDisplayID);
    void pageScreenDidChange(WebCore::PageIdentifier, WebCore::PlatformDisplayID, std::optional<unsigned> nominalFramesPerSecond);

private:
    struct DisplayProperties {
        WebCore::PlatformDisplayID displayId;
        WebCore::FramesPerSecond nominalFrameRate;
    };
    std::optional<DisplayProperties> displayProperties(WebCore::PageIdentifier) const;

    void startDisplayLink(WebCore::PageIdentifier);
    void stopDisplayLink(WebCore::PageIdentifier);
    void stopAllDisplayLinks();

    void updateTimelinesForPageId(WebCore::PageIdentifier);

    HashMap<WebCore::PageIdentifier, DisplayProperties> m_displayProperties;
    HashMap<WebCore::PlatformDisplayID, DisplayLinkObserverID, DefaultHash<WebCore::PlatformDisplayID>, WTF::UnsignedWithZeroKeyHashTraits<WebCore::PlatformDisplayID>> m_observers;
};

} // namespace WebKit

#endif // ENABLE(THREADED_ANIMATION_RESOLUTION)
