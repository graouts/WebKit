/*
 * Copyright (C) 2022 Igalia S.L. All rights reserved.
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

#if ENABLE(WEBASSEMBLY)

#include "CachedResourceHandle.h"
#include "CachedScript.h"
#include "CachedScriptFetcher.h"
#include "SharedBuffer.h"
#include "WebAssemblyCachedScriptSourceProvider.h"
#include "WebAssemblyScriptBufferSourceProvider.h"
#include <JavaScriptCore/SourceCode.h>
#include <JavaScriptCore/SourceProvider.h>
#include <wtf/RefPtr.h>
#include <wtf/URL.h>

namespace WebCore {

class WebAssemblyScriptSourceCode {
public:
    WebAssemblyScriptSourceCode(CachedScript* cachedScript, Ref<CachedScriptFetcher>&& scriptFetcher)
        : m_provider(WebAssemblyCachedScriptSourceProvider::create(cachedScript, WTFMove(scriptFetcher)))
        , m_code(m_provider.copyRef())
        , m_cachedScript(cachedScript)
    {
    }

    WebAssemblyScriptSourceCode(const ScriptBuffer& source, URL&& url, Ref<JSC::ScriptFetcher>&& scriptFetcher)
        : m_provider(WebAssemblyScriptBufferSourceProvider::create(source, WTFMove(url), WTFMove(scriptFetcher)))
        , m_code(m_provider.copyRef())
    {
    }

    const JSC::SourceCode& jsSourceCode() const { return m_code; }

private:
    const Ref<JSC::SourceProvider> m_provider;
    JSC::SourceCode m_code;
    CachedResourceHandle<CachedScript> m_cachedScript;
};

} // namespace WebCore

#endif // ENABLE(WEBASSEMBLY)
