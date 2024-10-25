/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include <WebCore/AcceleratedTimeline.h>
#include <WebCore/WebAnimationTime.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/UUID.h>

namespace WebKit {

class RemoteAnimationTimeline : public RefCounted<RemoteAnimationTimeline> {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(RemoteAnimationTimeline);
public:
    const WTF::UUID& identifier() const { return m_identifier; }
    std::optional<WebCore::WebAnimationTime> duration() const { return m_duration; }

    virtual std::optional<WebCore::WebAnimationTime> currentTime(MonotonicTime) const { return std::nullopt; }

    virtual ~RemoteAnimationTimeline() = default;

protected:
    explicit RemoteAnimationTimeline(const WebCore::AcceleratedTimeline&);

private:
    WTF::UUID m_identifier;
    std::optional<WebCore::WebAnimationTime> m_duration;
};

} // namespace WebKit

#endif // ENABLE(THREADED_ANIMATION_RESOLUTION)
