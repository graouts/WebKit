/*
 * Copyright (C) 2008-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Cameron Zwarich <cwzwarich@uwaterloo.ca>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "GlobalCodeBlock.h"
#include "UnlinkedModuleProgramCodeBlock.h"

namespace JSC {

class ModuleProgramCodeBlock final : public GlobalCodeBlock {
public:
    typedef GlobalCodeBlock Base;
    DECLARE_INFO;

    template<typename, SubspaceAccess>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return &vm.codeBlockSpace();
    }

    static ModuleProgramCodeBlock* create(VM& vm, CopyParsedBlockTag, ModuleProgramCodeBlock& other)
    {
        ModuleProgramCodeBlock* instance = new (NotNull, allocateCell<ModuleProgramCodeBlock>(vm))
            ModuleProgramCodeBlock(vm, vm.moduleProgramCodeBlockStructure.get(), CopyParsedBlock, other);
        instance->finishCreation(vm, CopyParsedBlock, other);
        return instance;
    }

    static ModuleProgramCodeBlock* create(VM& vm, ModuleProgramExecutable* ownerExecutable, UnlinkedModuleProgramCodeBlock* unlinkedCodeBlock, JSScope* scope)
    {
        ModuleProgramCodeBlock* instance = new (NotNull, allocateCell<ModuleProgramCodeBlock>(vm))
            ModuleProgramCodeBlock(vm, vm.moduleProgramCodeBlockStructure.get(), ownerExecutable, unlinkedCodeBlock, scope);
        if (!instance->finishCreation(vm, ownerExecutable, unlinkedCodeBlock, scope))
            return nullptr;
        return instance;
    }

    inline static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

private:
    ModuleProgramCodeBlock(VM& vm, Structure* structure, CopyParsedBlockTag, ModuleProgramCodeBlock& other)
        : GlobalCodeBlock(vm, structure, CopyParsedBlock, other)
    {
    }

    ModuleProgramCodeBlock(VM& vm, Structure* structure, ModuleProgramExecutable* ownerExecutable, UnlinkedModuleProgramCodeBlock* unlinkedCodeBlock, JSScope* scope)
        : GlobalCodeBlock(vm, structure, ownerExecutable, unlinkedCodeBlock, scope)
    {
    }
};
static_assert(sizeof(ModuleProgramCodeBlock) == sizeof(CodeBlock), "Subclasses of CodeBlock should be the same size to share IsoSubspace");

} // namespace JSC
