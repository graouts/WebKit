/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "ApplePayButtonPart.h"

#if ENABLE(APPLE_PAY)

#include "ControlFactory.h"

namespace WebCore {

Ref<ApplePayButtonPart> ApplePayButtonPart::create()
{
    return adoptRef(*new ApplePayButtonPart(ApplePayButtonType::Plain, ApplePayButtonStyle::White, { }));
}

Ref<ApplePayButtonPart> ApplePayButtonPart::create(ApplePayButtonType buttonType, ApplePayButtonStyle buttonStyle, const String& locale)
{
    return adoptRef(*new ApplePayButtonPart(buttonType, buttonStyle, locale));
}

ApplePayButtonPart::ApplePayButtonPart(ApplePayButtonType buttonType, ApplePayButtonStyle buttonStyle, const String& locale)
    : ControlPart(StyleAppearance::ApplePayButton)
    , m_buttonType(buttonType)
    , m_buttonStyle(buttonStyle)
    , m_locale(locale)
{
}

std::unique_ptr<PlatformControl> ApplePayButtonPart::createPlatformControl()
{
    return controlFactory().createPlatformApplePayButton(*this);
}

} // namespace WebCore

#endif // ENABLE(APPLE_PAY)
