// Instruction.h 
#pragma once

#include <cstdint>
#include <string>

struct Instruction {
	uint32_t rva;
	uint64_t address;
	uint8_t length;
	std::string text;
};