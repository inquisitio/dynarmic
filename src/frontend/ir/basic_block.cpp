/* This file is part of the dynarmic project.
 * Copyright (c) 2016 MerryMage
 * This software may be used and distributed according to the terms of the GNU
 * General Public License version 2 or any later version.
 */

#include <algorithm>
#include <initializer_list>
#include <map>
#include <string>

#include "common/assert.h"
#include "common/string_util.h"
#include "frontend/ir/basic_block.h"
#include "frontend/ir/opcodes.h"

namespace Dynarmic {
namespace IR {

void Block::AppendNewInst(Opcode opcode, std::initializer_list<IR::Value> args) {
    IR::Inst* inst = new(instruction_alloc_pool->Alloc()) IR::Inst(opcode);
    DEBUG_ASSERT(args.size() == inst->NumArgs());

    std::for_each(args.begin(), args.end(), [&inst, index = size_t(0)](const auto& arg) mutable {
        inst->SetArg(index, arg);
        index++;
    });

    instructions.push_back(inst);
}

Arm::LocationDescriptor Block::Location() const {
    return location;
}

Arm::Cond Block::GetCondition() const {
    return cond;
}

void Block::SetCondition(Arm::Cond condition) {
    cond = condition;
}

Arm::LocationDescriptor Block::ConditionFailedLocation() const {
    return cond_failed.get();
}

void Block::SetConditionFailedLocation(Arm::LocationDescriptor location) {
    cond_failed = location;
}

size_t& Block::ConditionFailedCycleCount() {
    return cond_failed_cycle_count;
}

const size_t& Block::ConditionFailedCycleCount() const {
    return cond_failed_cycle_count;
}

bool Block::HasConditionFailedLocation() const {
    return cond_failed.is_initialized();
}

Block::InstructionList& Block::Instructions() {
    return instructions;
}

const Block::InstructionList& Block::Instructions() const {
    return instructions;
}

Terminal Block::GetTerminal() const {
    return terminal;
}

void Block::SetTerminal(Terminal term) {
    ASSERT_MSG(!HasTerminal(), "Terminal has already been set.");
    terminal = term;
}

bool Block::HasTerminal() const {
    return terminal.which() != 0;
}

size_t& Block::CycleCount() {
    return cycle_count;
}

const size_t& Block::CycleCount() const {
    return cycle_count;
}

static std::string LocDescToString(const Arm::LocationDescriptor& loc) {
    return Common::StringFromFormat("{%u,%s,%s,%u}",
                                    loc.PC(),
                                    loc.TFlag() ? "T" : "!T",
                                    loc.EFlag() ? "E" : "!E",
                                    loc.FPSCR().Value());
}

static std::string TerminalToString(const Terminal& terminal_variant) {
    switch (terminal_variant.which()) {
    case 1: {
        auto terminal = boost::get<IR::Term::Interpret>(terminal_variant);
        return Common::StringFromFormat("Interpret{%s}", LocDescToString(terminal.next).c_str());
    }
    case 2: {
        return Common::StringFromFormat("ReturnToDispatch{}");
    }
    case 3: {
        auto terminal = boost::get<IR::Term::LinkBlock>(terminal_variant);
        return Common::StringFromFormat("LinkBlock{%s}", LocDescToString(terminal.next).c_str());
    }
    case 4: {
        auto terminal = boost::get<IR::Term::LinkBlockFast>(terminal_variant);
        return Common::StringFromFormat("LinkBlockFast{%s}", LocDescToString(terminal.next).c_str());
    }
    case 5: {
        return Common::StringFromFormat("PopRSBHint{}");
    }
    case 6: {
        auto terminal = boost::get<IR::Term::If>(terminal_variant);
        return Common::StringFromFormat("If{%s, %s, %s}", CondToString(terminal.if_), TerminalToString(terminal.then_).c_str(), TerminalToString(terminal.else_).c_str());
    }
    case 7: {
        auto terminal = boost::get<IR::Term::CheckHalt>(terminal_variant);
        return Common::StringFromFormat("CheckHalt{%s}", TerminalToString(terminal.else_).c_str());
    }
    default:
        return "<invalid terminal>";
    }
}

std::string DumpBlock(const IR::Block& block) {
    std::string ret;

    ret += Common::StringFromFormat("Block: location=%s\n", LocDescToString(block.Location()).c_str());
    ret += Common::StringFromFormat("cycles=%zu", block.CycleCount());
    ret += Common::StringFromFormat(", entry_cond=%s", Arm::CondToString(block.GetCondition(), true));
    if (block.GetCondition() != Arm::Cond::AL) {
        ret += Common::StringFromFormat(", cond_fail=%s", LocDescToString(block.ConditionFailedLocation()).c_str());
    }
    ret += "\n";

    std::map<const IR::Inst*, size_t> inst_to_index;
    size_t index = 0;

    const auto arg_to_string = [&inst_to_index](const IR::Value& arg) -> std::string {
        if (arg.IsEmpty()) {
            return "<null>";
        } else if (!arg.IsImmediate()) {
            return Common::StringFromFormat("%%%zu", inst_to_index.at(arg.GetInst()));
        }
        switch (arg.GetType()) {
        case Type::U1:
            return Common::StringFromFormat("#%s", arg.GetU1() ? "1" : "0");
        case Type::U8:
            return Common::StringFromFormat("#%u", arg.GetU8());
        case Type::U32:
            return Common::StringFromFormat("#%#x", arg.GetU32());
        case Type::RegRef:
            return Arm::RegToString(arg.GetRegRef());
        case Type::ExtRegRef:
            return Arm::ExtRegToString(arg.GetExtRegRef());
        default:
            return "<unknown immediate type>";
        }
    };

    for (const auto& inst : block) {
        const Opcode op = inst.GetOpcode();

        if (GetTypeOf(op) != Type::Void) {
            ret += Common::StringFromFormat("%%%-5zu = ", index);
        } else {
            ret += "         "; // '%00000 = ' -> 1 + 5 + 3 = 9 spaces
        }

        ret += GetNameOf(op);

        const size_t arg_count = GetNumArgsOf(op);
        for (size_t arg_index = 0; arg_index < arg_count; arg_index++) {
            const Value arg = inst.GetArg(arg_index);

            ret += arg_index != 0 ? ", " : " ";
            ret += arg_to_string(arg);

            Type actual_type = arg.GetType();
            Type expected_type = GetArgTypeOf(op, arg_index);
            if (!AreTypesCompatible(actual_type, expected_type)) {
                ret += Common::StringFromFormat("<type error: %s != %s>", GetNameOf(actual_type), GetNameOf(expected_type));
            }
        }

        ret += "\n";
        inst_to_index[&inst] = index++;
    }

    ret += "terminal = " + TerminalToString(block.GetTerminal()) + "\n";

    return ret;
}

} // namespace IR
} // namespace Dynarmic
