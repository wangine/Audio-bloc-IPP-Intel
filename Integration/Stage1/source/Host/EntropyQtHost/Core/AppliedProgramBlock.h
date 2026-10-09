#pragma once
#include <entropy/crossover_program_abi.h>
namespace entropy::host {
// Engine-thread owner. Unsupported modules use the separately marked legacy
// path; a negotiated module must return a fresh, valid result for every block.
class AppliedProgramBlock final {
public:
    unsigned inputsForRequest(bool requestedCinema, bool supported) const noexcept {
        const unsigned requested = requestedCinema ? 2u : 1u;
        return supported ? requested | (valid_ ? value_.required_inputs : 3u) : requested;
    }
    bool accept(const EntropyCrossoverProgramBlock& block) noexcept {
        if (block.struct_size < sizeof(block) || block.frames != 1024 || block.program > 1 ||
            block.session == 0 || block.block_sequence == 0 || block.program_revision == 0 ||
            block.output_program > 1 || block.transitions_in_block > block.frames ||
            (block.transitions_in_block != 0 && block.first_transition_frame >= block.frames) ||
            block.required_inputs == 0 || (block.required_inputs & ~3u) != 0 ||
            (block.required_inputs & (block.program ? 2u : 1u)) == 0)
            return false;
        if (valid_ && (block.session < value_.session ||
            (block.session == value_.session && (block.block_sequence <= value_.block_sequence ||
                                                block.program_revision < value_.program_revision ||
                                                block.output_revision < value_.output_revision))))
            return false;
        value_ = block; valid_ = true; return true;
    }
    const EntropyCrossoverProgramBlock& value() const noexcept { return value_; }
private:
    EntropyCrossoverProgramBlock value_{};
    bool valid_ = false;
};
}
