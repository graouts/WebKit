# Copyright (C) 2018-2019 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

require_relative 'Argument'
require_relative 'Fits'
require_relative 'Metadata'

class Opcode
    attr_reader :id
    attr_reader :args
    attr_reader :metadata
    attr_reader :extras
    attr_reader :checkpoints

    module Size
        Narrow = "OpcodeSize::Narrow"
        Wide16 = "OpcodeSize::Wide16"
        Wide32 = "OpcodeSize::Wide32"
    end

    @@id = 0

    def self.id
        tid = @@id
        @@id = @@id + 1
        tid
    end

    def initialize(section, name, extras, args, metadata, metadata_initializers, tmps, checkpoints)
        @section = section
        @name = name
        @extras = extras || {}
        @metadata = Metadata.new metadata, metadata_initializers
        @args = args.map.with_index { |(arg_name, type), index| Argument.new arg_name, type, index } unless args.nil?
        @tmps = tmps
        @checkpoints = checkpoints.map { |(checkpoint, _)| checkpoint } unless checkpoints.nil?
    end

    def create_id!
        @id = self.class.id
    end

    def print_args(&block)
        return if @args.nil?
        @args.map(&block).join "\n"
    end

    def print_members(prefix, &block)
        return if @args.nil?
        @args.map(&block).map { |arg| "#{prefix}#{arg}" }.join "\n"
    end

    def capitalized_name
        (name.split('_').map do |s|
            s = s.capitalize
            if s.match(/^\d/)
                i = s.match(/^\d+/)[0].size
                s = s[0..i-1] + s[i..-1].capitalize
            end
            s
        end).join
    end

    def typed_args
        return if @args.nil?

        @args.map(&:create_param).unshift("").join(", ")
    end

    def typed_reference_args
        return if @args.nil?

        @args.map(&:create_reference_param).unshift("").join(", ")
    end

    def untyped_args
        return if @args.nil?

        @args.map(&:name).unshift("").join(", ")
    end

    def opcodeIDType
      @section.is_wasm? ? :WasmOpcodeID : :OpcodeID
    end

    def wide16
      @section.is_wasm? ? :wasm_wide16 : :op_wide16
    end

    def wide32
      @section.is_wasm? ? :wasm_wide32 : :op_wide32
    end

    def traits
      @section.is_wasm? ? "WasmOpcodeTraits" : "JSOpcodeTraits"
    end

    def type_prefix
      @section.is_wasm? ? "Wasm" : "JS"
    end

    def map_fields_with_size(prefix, size, &block)
        args = [Argument.new("opcodeID", opcodeIDType, 0)]
        args += @args.dup if @args
        unless @metadata.empty?
            args << @metadata.emitter_local
        end
        args.map { |arg| "#{prefix}#{block.call(arg, size)}" }
    end

    def map_operands_with_size(prefix, size, &block)
        args = []
        args += @args.dup if @args
        unless @metadata.empty?
            args << @metadata.emitter_local
        end
        args.map { |arg| "#{prefix}#{block.call(arg, size)}" }
    end

    def struct
        <<-EOF
struct #{capitalized_name} : public #{type_prefix}Instruction {
    #{opcodeID}
    #{lengthValue}
    #{temps}
    #{checkpointValues}


#{emitter}
#{dumper}
#{constructors}
#{setters}#{metadata_struct_and_accessor}
#{members}
};
#{checkpointSizeAssert}
EOF
    end

    def opcodeID
        "static constexpr #{opcodeIDType} opcodeID = #{name};"
    end

    def lengthValue
        "static constexpr size_t length = #{length};"
    end

    def checkpointValues
        return if @checkpoints.nil?

        ["enum Checkpoints : uint8_t {"].concat(checkpoints.map{ |checkpoint| "        #{checkpoint}," }).concat(["        numberOfCheckpoints,", "    };"]).join("\n")
    end

    def checkpointSizeAssert
        return if @checkpoints.nil?

        "static_assert(#{capitalized_name}::length + 1 > #{capitalized_name}::numberOfCheckpoints, \"FullBytecodeLivess relies on the length of #{capitalized_name} being greater than the number of checkpoints\");"
    end

    def temps
        return if @tmps.nil?

        ["enum Tmps : uint8_t {"].concat(@tmps.map {|(tmp, type)| "        #{tmp},"}).push("    };").join("\n")
    end

    def emitter
        op_wide16 = Argument.new(wide16, opcodeIDType, 0)
        op_wide32 = Argument.new(wide32, opcodeIDType, 0)
        metadata_param = @metadata.empty? ? "" : ", #{@metadata.emitter_local.create_param}"
        metadata_arg = @metadata.empty? ? "" : ", #{@metadata.emitter_local.name}"
        <<-EOF.chomp
    template<typename BytecodeGenerator>
    static void emit(BytecodeGenerator* gen#{typed_args})
    {
        emitWithSmallestSizeRequirement<OpcodeSize::Narrow, BytecodeGenerator>(gen#{untyped_args});
    }
#{%{
    template<OpcodeSize __size, typename BytecodeGenerator, FitsAssertion shouldAssert = Assert>
    static bool emit(BytecodeGenerator* gen#{typed_args})
    {#{@metadata.create_emitter_local}
        return emit<__size, BytecodeGenerator, shouldAssert>(gen#{untyped_args}#{metadata_arg});
    }

    template<OpcodeSize __size, typename BytecodeGenerator>
    static bool checkWithoutMetadataID(BytecodeGenerator* gen#{typed_args})
    {
        decltype(gen->addMetadataFor(opcodeID)) __metadataID { };
        return checkImpl<__size, BytecodeGenerator>(gen#{untyped_args}#{metadata_arg});
    }
} unless @metadata.empty?}
    template<OpcodeSize __size, typename BytecodeGenerator, FitsAssertion shouldAssert = Assert, bool recordOpcode = true>
    static bool emit(BytecodeGenerator* gen#{typed_args}#{metadata_param})
    {
        bool didEmit = emitImpl<__size, recordOpcode, BytecodeGenerator>(gen#{untyped_args}#{metadata_arg});
        if (shouldAssert == Assert)
            ASSERT(didEmit);
        return didEmit;
    }

    template<OpcodeSize __size, typename BytecodeGenerator>
    static void emitWithSmallestSizeRequirement(BytecodeGenerator* gen#{typed_args})
    {
        #{@metadata.create_emitter_local}
        if (static_cast<unsigned>(__size) <= static_cast<unsigned>(OpcodeSize::Narrow)) {
            if (emit<OpcodeSize::Narrow, BytecodeGenerator, NoAssert, true>(gen#{untyped_args}#{metadata_arg}))
                return;
        }
        if (static_cast<unsigned>(__size) <= static_cast<unsigned>(OpcodeSize::Wide16)) {
            if (emit<OpcodeSize::Wide16, BytecodeGenerator, NoAssert, true>(gen#{untyped_args}#{metadata_arg}))
                return;
        }
        emit<OpcodeSize::Wide32, BytecodeGenerator, Assert, true>(gen#{untyped_args}#{metadata_arg});
    }

private:
    template<OpcodeSize __size, typename BytecodeGenerator>
    static bool checkImpl(BytecodeGenerator* gen#{typed_reference_args}#{metadata_param})
    {
        UNUSED_PARAM(gen);
        return #{map_fields_with_size("", "__size", &:fits_check).join "\n            && "}
            && (__size == OpcodeSize::Narrow ? #{Argument.new("opcodeID", opcodeIDType, 0).fits_check(Size::Narrow)} : #{Argument.new("opcodeID", opcodeIDType, 0).fits_check(Size::Wide16)})
            && (__size == OpcodeSize::Wide16 ? #{op_wide16.fits_check(Size::Narrow)} : true)
            && (__size == OpcodeSize::Wide32 ? #{op_wide32.fits_check(Size::Narrow)} : true);
    }

    template<OpcodeSize __size, bool recordOpcode, typename BytecodeGenerator>
    static bool emitImpl(BytecodeGenerator* gen#{typed_args}#{metadata_param})
    {
        #{!@checkpoints.nil? ? "gen->setUsesCheckpoints();" : ""}
        if (__size == OpcodeSize::Wide16)
            gen->alignWideOpcode16();
        else if (__size == OpcodeSize::Wide32)
            gen->alignWideOpcode32();
        if (checkImpl<__size>(gen#{untyped_args}#{metadata_arg})) {
            if (recordOpcode)
                gen->recordOpcode(opcodeID);
            if (__size == OpcodeSize::Wide16)
                #{op_wide16.fits_write Size::Narrow}
            else if (__size == OpcodeSize::Wide32)
                #{op_wide32.fits_write Size::Narrow}
            #{Argument.new("opcodeID", opcodeIDType, 0).fits_write "OpcodeIDWidthBySize<#{type_prefix}OpcodeTraits, __size>::opcodeIDSize"}
#{map_operands_with_size("            ", "__size", &:fits_write).join "\n"}
            return true;
        }
        return false;
    }

public:
EOF
    end

    def dumper
        <<-EOF
    void dump(BytecodeDumperBase<#{type_prefix}InstructionStream>* dumper, #{type_prefix}InstructionStream::Offset __location, int __sizeShiftAmount)
    {
        dumper->printLocationAndOp(__location, &"**#{@name}"[2 - __sizeShiftAmount]);
#{print_args { |arg|
<<-EOF.chomp
        dumper->dumpOperand("#{arg.name}", #{arg.field_name}, #{arg.index == 0});
EOF
    }}
    }
EOF
    end

    def constructors
        fields = (@args || []) + (@metadata.empty? ? [] : [@metadata])
        init = ->(size) { fields.empty?  ? "" : ": #{fields.map.with_index { |arg, i| arg.load_from_stream(i, size) }.join "\n        , " }" }

        <<-EOF
    #{capitalized_name}(const uint8_t* stream)
        #{init.call("OpcodeSize::Narrow")}
    {
        ASSERT_UNUSED(stream, static_cast<#{opcodeIDType}>(std::bit_cast<const typename OpcodeIDWidthBySize<#{type_prefix}OpcodeTraits, OpcodeSize::Narrow>::opcodeType*>(stream)[-1]) == opcodeID);
    }

    #{capitalized_name}(const uint16_t* stream)
        #{init.call("OpcodeSize::Wide16")}
    {
        ASSERT_UNUSED(stream, static_cast<#{opcodeIDType}>(std::bit_cast<const typename OpcodeIDWidthBySize<#{type_prefix}OpcodeTraits, OpcodeSize::Wide16>::opcodeType*>(stream)[-1]) == opcodeID);
    }


    #{capitalized_name}(const uint32_t* stream)
        #{init.call("OpcodeSize::Wide32")}
    {
        ASSERT_UNUSED(stream, static_cast<#{opcodeIDType}>(std::bit_cast<const typename OpcodeIDWidthBySize<#{type_prefix}OpcodeTraits, OpcodeSize::Wide32>::opcodeType*>(stream)[-1]) == opcodeID);
    }

    static #{capitalized_name} decode(const uint8_t* stream)
    {
        // A pointer is pointing to the first operand (opcode and prefix are not included).
        if (*stream == #{wide32})
            return { std::bit_cast<const uint32_t*>(stream + /* prefix */ 1 + OpcodeIDWidthBySize<#{type_prefix}OpcodeTraits, OpcodeSize::Wide32>::opcodeIDSize) };
        if (*stream == #{wide16})
            return { std::bit_cast<const uint16_t*>(stream + /* prefix */ 1 + OpcodeIDWidthBySize<#{type_prefix}OpcodeTraits, OpcodeSize::Wide16>::opcodeIDSize) };
        return { stream + OpcodeIDWidthBySize<#{type_prefix}OpcodeTraits, OpcodeSize::Narrow>::opcodeIDSize };
    }
EOF
    end

    def setters
        print_args { |a| a.setter(traits) }
    end

    def metadata_struct_and_accessor
        <<-EOF.chomp
#{@metadata.struct(self)}#{@metadata.accessor}
EOF
    end

    def members
        <<-EOF.chomp
#{print_members("    ", &:field)}#{@metadata.field("    ")}
EOF
    end

    def set_entry_address(id)
        "setEntryAddress(#{id}, _#{full_name})"
    end

    def set_entry_address_wide16(id)
        "setEntryAddressWide16(#{id}, _#{full_name}_wide16)"
    end

    def set_entry_address_wide32(id)
        "setEntryAddressWide32(#{id}, _#{full_name}_wide32)"
    end

    def struct_indices
        out = []
        out += @args.map(&:field_name) unless @args.nil?
        out << Metadata.field_name unless @metadata.empty?
        out.map.with_index do |name, index|
            "const unsigned #{capitalized_name}_#{name}_index = #{index};"
        end
    end

    def full_name
        "#{@section.config[:asm_prefix]}#{@section.config[:op_prefix]}#{@name}"
    end

    def name
        "#{@section.config[:op_prefix]}#{@name}"
    end

    def unprefixed_name
      @name
    end

    def length
        (@args.nil? ? 0 : @args.length) + (@metadata.empty? ? 0 : 1)
    end

    def self.dump_bytecode(name, opcode_traits, opcodes)
        type_prefix = (opcode_traits == :JSOpcodeTraits ? "JS" : (opcode_traits == :WasmOpcodeTraits ? "Wasm" : "Error#{opcode_traits}"))
        <<-EOF.chomp
void dump#{name}(BytecodeDumperBase<#{type_prefix}InstructionStream>* dumper, #{type_prefix}InstructionStream::Offset __location, const #{type_prefix}Instruction* __instruction)
{
    switch (__instruction->opcodeID()) {
#{opcodes.map { |op|
        <<-EOF.chomp
    case #{op.name}:
        __instruction->as<#{op.capitalized_name}>().dump(dumper, __location, __instruction->sizeShiftAmount());
        break;
EOF
    }.join "\n"}
    default:
        ASSERT_NOT_REACHED();
    }
}
EOF
    end
end
