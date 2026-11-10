#include "common/Types.h"
#include "loader/Loader.h"
#include "memory/MemoryManager.h"
#include "runtime/Process.h"


int main() {

	return 0;
}


/*
Load guest profile/schema
construct process
construct loader with process and guest environment
runtimeboostrap prepares process
	- request main image and ntldll mappings 
	- complete dependencies, relocations, imports
	- construct supported process/thread environment
	- finalize image protections


memory
- commit already committed page : succeeds for ordinary commitment within a valid reservation
- commit a mixed range : preserve existing pages and commit the missing pages
- decommit already reserved pages : succeeds for an otherwise valid range, those pages remain reserved
- query within an allocation : return the consective run beginning at the queried page with matching attributes
- query free address space : must be able to describe free ranges 
- explicit unaligned reservation : windows facing operation applies its address/range normalization rules




- Commit:
	- already committed : succeeds
	- mixed range of committed/reserved : preserve committed, commit reserved

- Decommit reserved pages:
	- Succeeds for the valid range of reserved pages

- Query
	- Return all pages w/ attributes, including those that are free

- Explicitly unaligned reservation ? handled by MM? 
*/