/*
 * Copyright (C) 2006-2023 Apple Inc. All rights reserved.
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

#include "ResourceErrorBase.h"

#include <wtf/RetainPtr.h>

OBJC_CLASS NSError;

namespace WebCore {

class ResourceError : public ResourceErrorBase {
public:
    ResourceError(Type type = Type::Null)
        : ResourceErrorBase(type)
    {
    }

    ResourceError(const String& domain, int errorCode, const URL& failingURL, const String& localizedDescription, Type type = Type::General, IsSanitized isSanitized = IsSanitized::No)
        : ResourceErrorBase(domain, errorCode, failingURL, localizedDescription, type, isSanitized)
    {
        ASSERT(domain != getNSURLErrorDomain());
        ASSERT(domain != getCFErrorDomainCFNetwork());
    }

    WEBCORE_EXPORT ResourceError(CFErrorRef error);

    WEBCORE_EXPORT CFErrorRef cfError() const;
    WEBCORE_EXPORT operator CFErrorRef() const;
    WEBCORE_EXPORT ResourceError(NSError *);
    WEBCORE_EXPORT NSError *nsError() const;
    WEBCORE_EXPORT operator NSError *() const;


    struct IPCData {
        Type type;
        RetainPtr<NSError> nsError;
        bool isSanitized;
    };
    WEBCORE_EXPORT static ResourceError fromIPCData(std::optional<IPCData>&&);
    WEBCORE_EXPORT std::optional<IPCData> ipcData() const;

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    WEBCORE_EXPORT bool blockedKnownTracker() const;
    WEBCORE_EXPORT String blockedTrackerHostName() const;
#endif

    WEBCORE_EXPORT ErrorRecoveryMethod errorRecoveryMethod() const;

    WEBCORE_EXPORT bool hasMatchingFailingURLKeys() const;

    static bool platformCompare(const ResourceError& a, const ResourceError& b);


private:
    friend class ResourceErrorBase;

    WEBCORE_EXPORT const String& getNSURLErrorDomain() const;
    WEBCORE_EXPORT const String& getCFErrorDomainCFNetwork() const;
    WEBCORE_EXPORT void mapPlatformError();

    void platformLazyInit();

    void doPlatformIsolatedCopy(const ResourceError&);

    mutable RetainPtr<NSError> m_platformError;
    bool m_dataIsUpToDate { true };
};

} // namespace WebCore
