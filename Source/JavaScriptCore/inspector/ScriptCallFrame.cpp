/*
 * Copyright (C) 2014-2024 Apple Inc. All rights reserved.
 * Copyright (c) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScriptCallFrame.h"

namespace Inspector {

ScriptCallFrame::ScriptCallFrame(const String& functionName, const String& scriptName, JSC::SourceID sourceID, JSC::LineColumn lineColumn)
    : m_functionName(functionName)
    , m_scriptName(scriptName)
    , m_sourceID(sourceID)
    , m_lineColumn(lineColumn)
{
}

ScriptCallFrame::ScriptCallFrame(const String& functionName, const String& scriptName, const String& preRedirectURL, JSC::SourceID sourceID, JSC::LineColumn lineColumn)
    : m_functionName(functionName)
    , m_scriptName(scriptName)
    , m_preRedirectURL(preRedirectURL)
    , m_sourceID(sourceID)
    , m_lineColumn(lineColumn)
{
}

ScriptCallFrame::~ScriptCallFrame() = default;

bool ScriptCallFrame::isEqual(const ScriptCallFrame& o) const
{
    // Ignore sourceID in isEqual in case of identical scripts executed multiple times
    // that would get different script identifiers, but are otherwise the same.
    return m_functionName == o.m_functionName
        && m_scriptName == o.m_scriptName
        && m_preRedirectURL == o.m_preRedirectURL
        && m_lineColumn == o.m_lineColumn;
}

bool ScriptCallFrame::isNative() const
{
    return m_scriptName == "[native code]"_s;
}

Ref<Protocol::Console::CallFrame> ScriptCallFrame::buildInspectorObject() const
{
    return Protocol::Console::CallFrame::create()
        .setFunctionName(m_functionName)
        .setUrl(m_scriptName)
        .setScriptId(String::number(m_sourceID))
        .setLineNumber(m_lineColumn.line)
        .setColumnNumber(m_lineColumn.column)
        .release();
}

} // namespace Inspector
