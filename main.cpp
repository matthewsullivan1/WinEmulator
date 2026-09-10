#include <iostream>
#include <windows.h>
#include <vector>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include <Zydis/Zydis.h>
#include <inttypes.h>
#include <unordered_map>

#include "PEFile.h"
#include "Disassembler.h"
#include "BasicBlock.h"
#include "Instruction.h"
#include "GuestMemory.h"

using namespace std;

int main() {

	GuestMemory memory;

	if (!memory.map(
		0x140000000,
		0x2500,
		Protection{ true, false, true }))
	{
		return 1;
	}

	return 0;
}
