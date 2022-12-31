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
#include "CSSMixValue.h"

namespace WebCore {

Ref<CSSMixValue> CSSMixValue::create(Ref<CSSPrimitiveValue>&& percentageValue, Ref<CSSValue>&& fromValue, Ref<CSSValue>&& toValue)
{
    return adoptRef(*new CSSMixValue(WTFMove(percentageValue), WTFMove(fromValue), WTFMove(toValue)));
}

inline CSSMixValue::CSSMixValue(Ref<CSSPrimitiveValue>&& percentageValue, Ref<CSSValue>&& fromValue, Ref<CSSValue>&& toValue)
    : CSSValue { MixClass }
    , m_percentageValue { WTFMove(percentageValue) }
    , m_fromValue { WTFMove(fromValue) }
    , m_toValue { WTFMove(toValue) }
{
}

CSSMixValue::~CSSMixValue() = default;

bool CSSMixValue::equals(const CSSMixValue& other) const
{
    return compareCSSValue(m_percentageValue, other.m_percentageValue) && compareCSSValue(m_fromValue, other.m_fromValue) && compareCSSValue(m_toValue, other.m_toValue);
}

String CSSMixValue::customCSSText() const
{
    return makeString("mix(", m_percentageValue->cssText(), "; ", m_fromValue->cssText(), "; ", m_toValue->cssText(), ")");
}

} // namespace WebCore
