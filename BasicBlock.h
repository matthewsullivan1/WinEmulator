// BasicBlock.h 
#pragma once

#include <vector>
#include <cstdint>

#include "Instruction.h"

struct BasicBlock {
	uint64_t startRva = 0;
	std::vector<Instruction> instructions{};
	std::vector<uint32_t> successors{};
};
