// Loader.cpp

#include "Loader.h"

LoaderStatus Loader::mapImage(PEFile& pe, ImageContext* ctx) {
	if (ctx == nullptr) {
		return LoaderStatus::InvalidImageContext;
	}

	// Reserve region for entire image
	uint64_t imageBase = 0;

	MemoryStatus status = mm_.reserve(
		pe.ntHeaders()->OptionalHeader.ImageBase, 
		pe.ntHeaders()->OptionalHeader.SizeOfImage, 
		RegionType::Image,
		&imageBase
	);
	
	if (status != MemoryStatus::Success) {
		return LoaderStatus::ImageReservationFailed;
	}

	// Commit header subregion
	status = mm_.commit(
		imageBase,
		pe.ntHeaders()->OptionalHeader.SizeOfHeaders,
		Protection{
			.r = true,
			.w = true,
			.x = false
		}
	);
	
	if (status != MemoryStatus::Success) {

		mm_.release(imageBase);

		return LoaderStatus::ImageHeaderCommitFailed;
	}

	// Write headers
	status = mm_.write(
		imageBase,
		pe.data().data(),
		pe.ntHeaders()->OptionalHeader.SizeOfHeaders
	);

	if (status != MemoryStatus::Success) {

		mm_.release(imageBase);

		return LoaderStatus::ImageHeaderMapFailed;
	}

	auto fileData = pe.data();

	for (const auto& section : pe.sections()) {
		uint64_t sectionDst = imageBase + section.VirtualAddress;

		auto sectionData = fileData.subspan(
			section.PointerToRawData,
			section.SizeOfRawData
		);

		size_t mappedSize = std::max(
			static_cast<size_t>(section.Misc.VirtualSize),
			static_cast<size_t>(section.SizeOfRawData)
		);

		status = mm_.commit(
			sectionDst,
			mappedSize,
			Protection{
				.r = true,
				.w = true,
				.x = false
			}
		);

		if (status != MemoryStatus::Success) {

			mm_.release(imageBase);

			return LoaderStatus::ImageSectionCommitFailed;
		}

		status = mm_.write(
			sectionDst,
			sectionData.data(),
			sectionData.size()
		);

		if (status != MemoryStatus::Success) {
			
			mm_.release(imageBase);

			return LoaderStatus::ImageSectionMapFailed;
		}
	}

	ctx->baseAddress = imageBase;
	ctx->entryPoint = imageBase + pe.entryPointRva();
	ctx->size = pe.ntHeaders()->OptionalHeader.SizeOfImage;

	return LoaderStatus::Success;
}