// Disassembler.h
#pragma once

#include <Zydis/Zydis.h>
#include <unordered_map>

#include "BasicBlock.h"
#include "Instruction.h"
#include "PEFile.h"



class Disassembler {
	
public:
	explicit Disassembler(const PEFile& pe);	
	void disassembleEntryPoint() const;
	void disassembleSection(const IMAGE_SECTION_HEADER& section) const;
	void disassembleRange(const uint8_t* data, size_t size, uint64_t runtimeAddress) const;
	BasicBlock buildBasicBlock(uint32_t startRva) const;
	std::unordered_map<uint32_t, BasicBlock> analyze()const;
	bool isTerminator(const ZydisDecodedInstruction& inst) const;


private:
	const PEFile& pe_;
	ZydisDecoder decoder_;
	ZydisFormatter formatter_;
};