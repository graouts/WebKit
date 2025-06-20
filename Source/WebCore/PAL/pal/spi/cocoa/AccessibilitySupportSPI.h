/*
 * Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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

DECLARE_SYSTEM_HEADER

#include <CoreFoundation/CoreFoundation.h>

#if USE(APPLE_INTERNAL_SDK)

// FIXME: Remove once rdar://131328679 is fixed and distributed.
IGNORE_WARNINGS_BEGIN("#warnings")
#include <AccessibilitySupport.h>
IGNORE_WARNINGS_END

#else

typedef CF_ENUM(int32_t, AXSIsolatedTreeMode)
{
    AXSIsolatedTreeModeOff = 0,
    AXSIsolatedTreeModeMainThread,
    AXSIsolatedTreeModeSecondaryThread,
};

#endif

WTF_EXTERN_C_BEGIN

AXSIsolatedTreeMode _AXSIsolatedTreeMode(void);
void _AXSSetIsolatedTreeMode(AXSIsolatedTreeMode);

extern CFStringRef kAXSEnhanceTextLegibilityChangedNotification;
Boolean _AXSEnhanceTextLegibilityEnabled();

WTF_EXTERN_C_END
