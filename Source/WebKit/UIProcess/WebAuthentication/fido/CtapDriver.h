/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_AUTHN)

#include <WebCore/AuthenticatorTransport.h>
#include <WebCore/FidoConstants.h>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

class CtapDriver : public RefCountedAndCanMakeWeakPtr<CtapDriver> {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(CtapDriver);
    WTF_MAKE_NONCOPYABLE(CtapDriver);
public:
    using ResponseCallback = Function<void(Vector<uint8_t>&&)>;

    virtual ~CtapDriver() = default;

    void setProtocol(fido::ProtocolVersion protocol) { m_protocol = protocol; }

    WebCore::AuthenticatorTransport transport() const { return m_transport; }
    fido::ProtocolVersion protocol() const { return m_protocol; }
    bool isCtap2Protocol() const { return fido::isCtap2Protocol(m_protocol); }
    void setMaxMsgSize(std::optional<uint32_t> maxMsgSize) { m_maxMsgSize = maxMsgSize; }
    bool isValidSize(size_t msgSize) { return !m_maxMsgSize || msgSize <= static_cast<size_t>(*m_maxMsgSize); }

    virtual void transact(Vector<uint8_t>&& data, ResponseCallback&&) = 0;
    virtual void cancel() { };
protected:
    explicit CtapDriver(WebCore::AuthenticatorTransport transport)
        : m_transport(transport)
    { }

private:
    fido::ProtocolVersion m_protocol { fido::ProtocolVersion::kCtap2 };
    WebCore::AuthenticatorTransport m_transport;
    std::optional<uint32_t> m_maxMsgSize;
};

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
