/*
 * Copyright (C) 2012-2025 Apple Inc. All rights reserved.
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

#ifndef InjectedBundleDOMWindowExtension_h
#define InjectedBundleDOMWindowExtension_h

#include "APIObject.h"
#include <wtf/RefPtr.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class DOMWindowExtension;

}

namespace WebKit {

class InjectedBundleScriptWorld;
class WebFrame;

class InjectedBundleDOMWindowExtension : public API::ObjectImpl<API::Object::Type::BundleDOMWindowExtension>, public CanMakeWeakPtr<InjectedBundleDOMWindowExtension> {
public:
    static Ref<InjectedBundleDOMWindowExtension> create(WebFrame*, InjectedBundleScriptWorld*);
    static InjectedBundleDOMWindowExtension* get(WebCore::DOMWindowExtension*);

    virtual ~InjectedBundleDOMWindowExtension();
    
    RefPtr<WebFrame> frame() const;
    InjectedBundleScriptWorld* world() const;

private:
    InjectedBundleDOMWindowExtension(WebFrame*, InjectedBundleScriptWorld*);

    const Ref<WebCore::DOMWindowExtension> m_coreExtension;
    mutable RefPtr<InjectedBundleScriptWorld> m_world;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::InjectedBundleDOMWindowExtension)
static bool isType(const API::Object& object) { return object.type() == API::Object::Type::BundleDOMWindowExtension; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // InjectedBundleDOMWindowExtension_h
