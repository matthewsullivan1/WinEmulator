// Process.cpp

#include "Process.h"
#include "windows/WinStructs.h"
#include <iostream>

using win::PEB;
using win::PEB_LDR_DATA;
using win::LDR_DATA_TABLE_ENTRY;
using win::LIST_ENTRY;
using win::UNICODE_STRING;



namespace emu {

	MemoryStatus Process::initializePeb() {

		uint64_t pebAddress;
		uint64_t ldrAddress;

		PEB peb;
		PEB_LDR_DATA ldr;

		MemoryStatus status = memoryManager_.allocate(0, sizeof(PEB), PageProtection::ReadWrite, RegionType::Private, &pebAddress);

		if (status != MemoryStatus::Success) {
			return status;
		}

		status = memoryManager_.allocate(0, sizeof(ldr), PageProtection::ReadWrite, RegionType::Private, &ldrAddress);

		if (status != MemoryStatus::Success) {
			return status;
		}

		pebAddress_ = pebAddress;
		peb.ImageBaseAddress = moduleManager_.mainModule()->baseAddress;
		peb.Ldr = ldrAddress;

		ldr.Length = sizeof(PEB_LDR_DATA);

		for (auto& module : moduleManager_.loadOrder()) {

			uint64_t entryAddress;
			LDR_DATA_TABLE_ENTRY entry;

			MemoryStatus status = memoryManager_.allocate(
				0,
				sizeof(LDR_DATA_TABLE_ENTRY),
				PageProtection::ReadWrite,
				RegionType::Private,
				&entryAddress
			);

			if (status != MemoryStatus::Success) {
				return status;
			}


			entry.DllBase = module->baseAddress;
			entry.EntryPoint = module->entryPoint;
			entry.SizeOfImage = module->sizeOfImage;

			std::wstring modulePath = module->path.wstring();
			std::wstring moduleName(module->name.begin(), module->name.end());

			UNICODE_STRING FullDllName;
			UNICODE_STRING BaseDllName;

			FullDllName.Length = static_cast<uint16_t>(sizeof(wchar_t) * modulePath.size());
			BaseDllName.Length = static_cast<uint16_t>(sizeof(wchar_t) * moduleName.size());

			FullDllName.MaximumLength = static_cast<uint16_t>(sizeof(wchar_t) * (modulePath.size() + 1));
			BaseDllName.MaximumLength = static_cast<uint16_t>(sizeof(wchar_t) * (moduleName.size() + 1));

			uint64_t pathBuf, nameBuf;

			status = memoryManager_.allocate(0, static_cast<size_t>(FullDllName.MaximumLength), PageProtection::ReadWrite, RegionType::Private, &pathBuf);
			if (status != MemoryStatus::Success) { return status; }

			status = memoryManager_.allocate(0, static_cast<size_t>(BaseDllName.MaximumLength), PageProtection::ReadWrite, RegionType::Private, &nameBuf);
			if (status != MemoryStatus::Success) { return status; }

			FullDllName.Buffer = pathBuf;
			BaseDllName.Buffer = nameBuf;

			status = memoryManager_.write(pathBuf, modulePath.c_str(), FullDllName.MaximumLength);
			if (status != MemoryStatus::Success) { return status; }

			status = memoryManager_.write(nameBuf, moduleName.c_str(), BaseDllName.MaximumLength);
			if (status != MemoryStatus::Success) { return status; }

			entry.FullDllName = FullDllName;
			entry.BaseDllName = BaseDllName;

			status = memoryManager_.write(entryAddress, &entry, sizeof(entry));
			if (status != MemoryStatus::Success) { return status; }

			printf("FullDllName:\t%ls\nBaseDllName:\t%ls\nVirtualAddress:\t%llu\n\n", modulePath.c_str(), moduleName.c_str(), entryAddress);

		}

		status = memoryManager_.write(pebAddress, &peb, sizeof(PEB));
		if (status != MemoryStatus::Success) { return status; }

		status = memoryManager_.write(ldrAddress, &ldr, sizeof(PEB_LDR_DATA));
		if (status != MemoryStatus::Success) return status;

		return MemoryStatus::Success;

	}

	//MemoryStatus Process::linkModulesList() {

	//	PEB peb{};
	//	PEB_LDR_DATA ldr{};
	//	MemoryStatus status;

	//	status = memoryManager_.read(pebAddress_, &peb, sizeof(PEB));
	//	if (status != MemoryStatus::Success) { return status; }

	//	status = memoryManager_.read(peb.Ldr, &ldr, sizeof(PEB_LDR_DATA));
	//	if (status != MemoryStatus::Success) { return status; }

	//	auto linkList = [&](size_t headOffset, size_t nodeOffset) -> MemoryStatus {

	//		uint64_t headAddress = peb.Ldr + headOffset;

	//		LIST_ENTRY head{};
	//		size_t count = modules().loadOrder().size();

	//		// empty circular list
	//		if (count == 0) {
	//			head.Flink = headAddress;
	//			head.Blink = headAddress;
	//		}
	//		else {
	//			auto nodeAddress = [&](size_t i) -> uint64_t {
	//				return loadOrder[i]->ldrEntryAddress + nodeOffset;
	//				};

	//			head.Flink = nodeAddress(0);
	//			head.Blink = nodeAddress(count - 1);

	//			for (size_t i = 0; i < count; ++i) {

	//				_GUEST_LIST_ENTRY node{};

	//				node.Blink = (i == 0)
	//					? headAddress
	//					: nodeAddress(i - 1);

	//				node.Flink = (i == count - 1)
	//					? headAddress
	//					: nodeAddress(i + 1);

	//				MemoryStatus result = memoryManager_.write(nodeAddress(i), &node, sizeof(node));
	//				if (result != MemoryStatus::Success) return result;
	//			}
	//		}

	//		return memoryManager_.write(headAddress, &head, sizeof(head));
	//		};

	//	status = linkList(
	//		offsetof(_GUEST_PEB_LDR_DATA, InLoadOrderModuleList),
	//		offsetof(_GUEST_LDR_DATA_TABLE_ENTRY, InLoadOrderLinks)
	//	);
	//	if (status != MemoryStatus::Success) return status;

	//	status = linkList(
	//		offsetof(_GUEST_PEB_LDR_DATA, InMemoryOrderModuleList),
	//		offsetof(_GUEST_LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks)
	//	);
	//	if (status != MemoryStatus::Success) return status;

	//	status = linkList(
	//		offsetof(_GUEST_PEB_LDR_DATA, InInitializationOrderModuleList),
	//		offsetof(_GUEST_LDR_DATA_TABLE_ENTRY, InInitializationOrderLinks)
	//	);
	//	if (status != MemoryStatus::Success) return status;

	//	// Debug verification
	//	//_GUEST_PEB peb{};
	//	//_GUEST_PEB_LDR_DATA ldr{};

	//	memoryManager_.read(pebAddress_, &peb, sizeof(peb));
	//	memoryManager_.read(peb.Ldr, &ldr, sizeof(ldr));

	//	auto verifyList = [&](const char* listName,
	//		size_t headOffset,
	//		size_t nodeOffset) {

	//			uint64_t headAddress = peb.Ldr + headOffset;

	//			_GUEST_LIST_ENTRY head{};
	//			memoryManager_.read(headAddress, &head, sizeof(head));

	//			std::cout << "\n" << listName << "\n";
	//			std::cout << "Head: 0x" << std::hex << headAddress
	//				<< " Flink: 0x" << head.Flink
	//				<< " Blink: 0x" << head.Blink << "\n";

	//			uint64_t current = head.Flink;
	//			uint64_t previous = headAddress;
	//			size_t count = 0;
	//			bool valid = true;

	//			while (current != headAddress && count < loadOrder_.size()) {

	//				_GUEST_LIST_ENTRY node{};
	//				MemoryStatus status = memoryManager_.read(current, &node, sizeof(node));

	//				if (status != MemoryStatus::Success) {
	//					valid = false;
	//					break;
	//				}

	//				// Recover the containing LDR_DATA_TABLE_ENTRY address.
	//				uint64_t entryAddress = current - nodeOffset;

	//				_GUEST_LDR_DATA_TABLE_ENTRY entry{};
	//				status = memoryManager_.read(entryAddress, &entry, sizeof(entry));

	//				if (status != MemoryStatus::Success) {
	//					valid = false;
	//					break;
	//				}

	//				if (node.Blink != previous) {
	//					valid = false;
	//					std::cout << "  ERROR: Incorrect Blink\n";
	//				}

	//				std::cout << "  [" << std::dec << count << "]"
	//					<< " DllBase: 0x" << std::hex << entry.DllBase
	//					<< " Node: 0x" << current
	//					<< " Flink: 0x" << node.Flink
	//					<< " Blink: 0x" << node.Blink << "\n";

	//				previous = current;
	//				current = node.Flink;
	//				++count;
	//			}

	//			if (current != headAddress) {
	//				valid = false;
	//				std::cout << "  ERROR: List did not return to head\n";
	//			}

	//			if (head.Blink != previous) {
	//				valid = false;
	//				std::cout << "  ERROR: Head Blink does not match last node\n";
	//			}

	//			if (count != loadOrder_.size()) {
	//				valid = false;
	//				std::cout << "  ERROR: Unexpected module count\n";
	//			}

	//			std::cout << (valid ? "  PASS\n" : "  FAIL\n");
	//		};

	//	verifyList(
	//		"InLoadOrderModuleList",
	//		offsetof(_GUEST_PEB_LDR_DATA, InLoadOrderModuleList),
	//		offsetof(_GUEST_LDR_DATA_TABLE_ENTRY, InLoadOrderLinks)
	//	);

	//	verifyList(
	//		"InMemoryOrderModuleList",
	//		offsetof(_GUEST_PEB_LDR_DATA, InMemoryOrderModuleList),
	//		offsetof(_GUEST_LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks)
	//	);

	//	verifyList(
	//		"InInitializationOrderModuleList",
	//		offsetof(_GUEST_PEB_LDR_DATA, InInitializationOrderModuleList),
	//		offsetof(_GUEST_LDR_DATA_TABLE_ENTRY, InInitializationOrderLinks)
	//	);

	//	std::cout << std::dec;

	//	return MemoryStatus::Success;
	//}
}