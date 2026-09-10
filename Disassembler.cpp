// Disassembler.cpp

#include <stdexcept>
#include <Windows.h>
#include <inttypes.h>
#include <algorithm>
#include <queue>
#include <unordered_set>
#include <unordered_map>


#include "Disassembler.h"
#include "BasicBlock.h"
#include "Instruction.h"

Disassembler::Disassembler(const PEFile& pe) : pe_(pe){
		
	if (ZydisDecoderInit(&decoder_, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64) != ZYAN_STATUS_SUCCESS) {
		throw std::runtime_error("Error initializing decoder");
	}

	if (ZydisFormatterInit(&formatter_, ZYDIS_FORMATTER_STYLE_INTEL) != ZYAN_STATUS_SUCCESS) {
		throw std::runtime_error("Error initializing formatter");
	}
}

void Disassembler::disassembleSection(const IMAGE_SECTION_HEADER& section) const {
	
    auto data = pe_.data();

    size_t offset = section.PointerToRawData; // This gives us the file offset (within pe.data_.data()) of the section
    size_t remaining = section.SizeOfRawData; // This is the length of the section's raw data within the data array.
    // Note that statically before runtime the section will most likely have some uninitialized data. so we need to 
    // start at the entry point, and look for basic blocks rather than a flat disassembly of the entire section

    ZyanU64 runtimeAddress =
        pe_.imageBase() + section.VirtualAddress;

    if (offset >= data.size()) {
        return;
    }

    // Don't allow malformed section metadata to go past the file.
    remaining = min(
        remaining,
        data.size() - offset
    );

    ZydisDisassembledInstruction instruction;

    while (remaining > 0 &&
        ZYAN_SUCCESS(ZydisDisassembleIntel(
            ZYDIS_MACHINE_MODE_LONG_64,
            runtimeAddress,
            data.data() + offset,
            remaining,
            &instruction
        )))
    {
        printf(
            "%016" PRIX64 "  %s\n",
            runtimeAddress,
            instruction.text
        );

        offset += instruction.info.length;
        runtimeAddress += instruction.info.length;
        remaining -= instruction.info.length;
    }
}

BasicBlock Disassembler::buildBasicBlock(uint32_t startRva) const {
    
    auto data = pe_.data();
    BasicBlock b;

    auto offsetOpt = pe_.rvaToFileOffset(startRva);
    if (!offsetOpt) {
        printf("rvaToFileOffset error\n");
        return b;
    }

    size_t offset = *offsetOpt;

    b.startRva = startRva;
    uint64_t runtimeAddress = pe_.imageBase() + startRva;
    size_t maxLen = data.size() - offset;

    ZydisDisassembledInstruction i;

    while (ZYAN_SUCCESS(ZydisDisassembleIntel(
        ZYDIS_MACHINE_MODE_LONG_64,
        runtimeAddress,
        data.data() + offset,
        maxLen,
        &i
    )))
    {
        printf(
            "%016" PRIX64 "  %s\n",
            runtimeAddress,
            i.text
        );

        uint32_t instructionRva =
            static_cast<uint32_t>(runtimeAddress - pe_.imageBase());

        b.instructions.emplace_back(
            instructionRva,
            runtimeAddress,
            i.info.length,
            i.text
        );

        // Handle terminator while runtimeAddress
        // still refers to THIS instruction.
        if (isTerminator(i.info))
        {
            switch (i.info.meta.category)
            {
            case ZYDIS_CATEGORY_COND_BR:
            {
                const auto& operand = i.operands[0];

                // Taken edge
                if (operand.type == ZYDIS_OPERAND_TYPE_IMMEDIATE)
                {
                    ZyanU64 targetAddress;

                    if (ZYAN_SUCCESS(ZydisCalcAbsoluteAddress(
                        &i.info,
                        &operand,
                        runtimeAddress,
                        &targetAddress)))
                    {
                        uint32_t targetRva =
                            static_cast<uint32_t>(
                                targetAddress - pe_.imageBase()
                                );

                        b.successors.push_back(targetRva);
                    }
                }

                // Not-taken / fallthrough edge
                uint32_t fallthroughRva =
                    static_cast<uint32_t>(
                        runtimeAddress +
                        i.info.length -
                        pe_.imageBase()
                        );

                b.successors.push_back(fallthroughRva);

                break;
            }

            case ZYDIS_CATEGORY_UNCOND_BR:
            {
                const auto& operand = i.operands[0];

                if (operand.type == ZYDIS_OPERAND_TYPE_IMMEDIATE)
                {
                    ZyanU64 targetAddress;

                    if (ZYAN_SUCCESS(ZydisCalcAbsoluteAddress(
                        &i.info,
                        &operand,
                        runtimeAddress,
                        &targetAddress)))
                    {
                        uint32_t targetRva =
                            static_cast<uint32_t>(
                                targetAddress - pe_.imageBase()
                                );

                        b.successors.push_back(targetRva);
                    }
                }

                // Indirect JMP ignored for now
                break;
            }

            case ZYDIS_CATEGORY_RET:
                break;
            }

            break;
        }

        // Only advance if we're continuing this block.
        offset += i.info.length;
        runtimeAddress += i.info.length;
        maxLen -= i.info.length;
    }

    return b;
}

bool Disassembler::isTerminator(const ZydisDecodedInstruction &inst) const {
    switch (inst.meta.category) {
    case ZYDIS_CATEGORY_COND_BR:
    case ZYDIS_CATEGORY_UNCOND_BR:
    case ZYDIS_CATEGORY_RET:
        return true;

    default:
        return false;
    }
}

std::unordered_map<uint32_t, BasicBlock> Disassembler::analyze() const {
    std::queue<uint32_t> pending; 
    std::unordered_set<uint32_t> visited;
    std::unordered_map<uint32_t, BasicBlock> nodes; 


    size_t entryRva = pe_.ntHeaders()->OptionalHeader.AddressOfEntryPoint;
    pending.push(entryRva);

    while (!pending.empty()) {
        uint32_t currentRva = pending.front();
        pending.pop();


        // Node has already been mapped
        if (visited.contains(currentRva)) {
            continue;
        }

        visited.insert(currentRva);
        BasicBlock b = buildBasicBlock(currentRva);

        for (uint32_t successor : b.successors) {
            if (!visited.contains(successor)) {
                pending.push(successor);
            }
        }

        nodes.emplace(currentRva, std::move(b));
    }

    return nodes; 
}







/*
Unconditional jump -> terminator, block's successor is jump target

Conditional branch -> terminator, block's successors are the jump target (1) and next instruction (2) 

Ret -> terminator, no successors 

Call -> not a terminator

Indirect jump -> terminator, however we dont know where the jump target is until runtime. Successor is unresolved



*/
