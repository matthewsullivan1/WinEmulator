// BasicBlock.h 
#pragma once

#include <vector>
#include <cstdint>

#include "Instruction.h"

struct BasicBlock {
	uint64_t startRva;
	std::vector<Instruction> instructions;
	std::vector<uint32_t> successors;
};
