/* This file is part of the dynarmic project.
 * Copyright (c) 2016 MerryMage
 * This software may be used and distributed according to the terms of the GNU
 * General Public License version 2 or any later version.
 */

#pragma once

#include <initializer_list>
#include <memory>
#include <string>

#include <boost/optional.hpp>

#include "common/common_types.h"
#include "common/intrusive_list.h"
#include "common/memory_pool.h"
#include "frontend/arm_types.h"
#include "frontend/ir/microinstruction.h"
#include "frontend/ir/terminal.h"
#include "frontend/ir/value.h"

namespace Dynarmic {
namespace IR {

enum class Opcode;

/**
 * A basic block. It consists of zero or more instructions followed by exactly one terminal.
 * Note that this is a linear IR and not a pure tree-based IR: i.e.: there is an ordering to
 * the microinstructions. This only matters before chaining is done in order to correctly
 * order memory accesses.
 */
class Block final {
public:
    using InstructionList        = Common::IntrusiveList<Inst>;
    using size_type              = InstructionList::size_type;
    using iterator               = InstructionList::iterator;
    using const_iterator         = InstructionList::const_iterator;
    using reverse_iterator       = InstructionList::reverse_iterator;
    using const_reverse_iterator = InstructionList::const_reverse_iterator;

    explicit Block(const Arm::LocationDescriptor& location) : location(location) {}

    bool                   empty()   const { return instructions.empty();   }
    size_type              size()    const { return instructions.size();    }

    Inst&                  front()         { return instructions.front();   }
    const Inst&            front()   const { return instructions.front();   }

    Inst&                  back()          { return instructions.back();    }
    const Inst&            back()    const { return instructions.back();    }

    iterator               begin()         { return instructions.begin();   }
    const_iterator         begin()   const { return instructions.begin();   }
    iterator               end()           { return instructions.end();     }
    const_iterator         end()     const { return instructions.end();     }

    reverse_iterator       rbegin()        { return instructions.rbegin();  }
    const_reverse_iterator rbegin()  const { return instructions.rbegin();  }
    reverse_iterator       rend()          { return instructions.rend();    }
    const_reverse_iterator rend()    const { return instructions.rend();    }

    const_iterator         cbegin()  const { return instructions.cbegin();  }
    const_iterator         cend()    const { return instructions.cend();    }

    const_reverse_iterator crbegin() const { return instructions.crbegin(); }
    const_reverse_iterator crend()   const { return instructions.crend();   }

    /**
     * Appends a new instruction to this basic block,
     * handling any allocations necessary to do so.
     *
     * @param op   Opcode representing the instruction to add.
     * @param args A sequence of Value instances used as arguments for the instruction.
     */
    void AppendNewInst(Opcode op, std::initializer_list<Value> args);

    /// Gets the starting location for this basic block.
    Arm::LocationDescriptor Location() const;

    /// Gets the condition required to pass in order to execute this block.
    Arm::Cond GetCondition() const;
    /// Sets the condition required to pass in order to execute this block.
    void SetCondition(Arm::Cond condition);

    /// Gets the location of the block to execute if the predicated condition fails.
    Arm::LocationDescriptor ConditionFailedLocation() const;
    /// Sets the location of the block to execute if the predicated condition fails.
    void SetConditionFailedLocation(Arm::LocationDescriptor location);
    /// Determines whether or not a prediated condition failure block is present.
    bool HasConditionFailedLocation() const;

    /// Gets a mutable reference to the condition failed cycle count.
    size_t& ConditionFailedCycleCount();
    /// Gets an immutable reference to the condition failed cycle count.
    const size_t& ConditionFailedCycleCount() const;

    /// Gets a mutable reference to the instruction list for this basic block.
    InstructionList& Instructions();
    /// Gets an immutable reference to the instruction list for this basic block.
    const InstructionList& Instructions() const;

    /// Gets the terminal instruction for this basic block.
    Terminal GetTerminal() const;
    /// Sets the terminal instruction for this basic block.
    void SetTerminal(Terminal term);
    /// Determines whether or not this basic block has a terminal instruction.
    bool HasTerminal() const;

    /// Gets a mutable reference to the cycle count for this basic block.
    size_t& CycleCount();
    /// Gets an immutable reference to the cycle count for this basic block.
    const size_t& CycleCount() const;

private:
    /// Description of the starting location of this block
    Arm::LocationDescriptor location;
    /// Conditional to pass in order to execute this block
    Arm::Cond cond = Arm::Cond::AL;
    /// Block to execute next if `cond` did not pass.
    boost::optional<Arm::LocationDescriptor> cond_failed = {};
    /// Number of cycles this block takes to execute if the conditional fails.
    size_t cond_failed_cycle_count = 0;

    /// List of instructions in this block.
    InstructionList instructions;
    /// Memory pool for instruction list
    std::unique_ptr<Common::Pool> instruction_alloc_pool = std::make_unique<Common::Pool>(sizeof(Inst), 4096);
    /// Terminal instruction of this block.
    Terminal terminal = Term::Invalid{};

    /// Number of cycles this block takes to execute.
    size_t cycle_count = 0;
};

/// Returns a string representation of the contents of block. Intended for debugging.
std::string DumpBlock(const IR::Block& block);

} // namespace IR
} // namespace Dynarmic
