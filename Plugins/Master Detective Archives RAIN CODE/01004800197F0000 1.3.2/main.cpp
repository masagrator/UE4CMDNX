#include <switch_min.h>

#include "saltysd/saltysd_core.h"
#include "saltysd/saltysd_ipc.h"
#include "saltysd/saltysd_dynamic.h"
#include <string>

extern "C" {

	struct fString16 {
		const char16_t* command;
		int character_count;
		char reserved[4];
	} string16, dummy16;

	struct fString32 {
		const char32_t* command;
		int character_count;
		char reserved[4];
	} string32, dummy32;

	extern u32 __start__;

	static char g_heap[0x4000];

	void __libnx_init(void* ctx, Handle main_thread, void* saved_lr);
	void __attribute__((weak)) NORETURN __libnx_exit(int rc);
	void __nx_exit(int, void*);
	void __libc_fini_array(void);
	void __libc_init_array(void);
	extern void _ZN2nn2os9WaitEventEPNS0_9EventTypeE(void* EventType) LINKABLE;
}

u32 __nx_applet_type = AppletType_None;

SharedMemory _sharedmemory = {};
Handle remoteSharedMemory = 0;
ptrdiff_t SharedMemoryOffset = 1234;
size_t main_size = 0;
Handle orig_main_thread;
void* orig_ctx;
void* orig_saved_lr;

const char* ver = "1.0.0";
bool* checkFlag_shared = 0;
char16_t* string_shared = 0;
uint64_t main_address = 0;
uint64_t ConsoleCommandAddress = 0;
uint32_t instanceOffset = 0x9DE9B0; // Master Detective Archives: RAIN CODE 1.3.2
bool U32Flag = false;

typedef fString16* (*ConsoleCommandU16)(void* instance, fString16* Fstring, bool writeToLog, const void* dummy, const void* dummy2, const void* dummy3, const void* dummy4, const void* dummy5, const void* validPointerToNothing);
typedef fString32* (*ConsoleCommandU32)(void* instance, fString32* Fstring, bool writeToLog, const void* dummy, const void* dummy2, const void* dummy3, const void* dummy4, const void* dummy5, const void* validPointerToNothing);
typedef void* (*GetInstanceOffset)(void);

void __libnx_init(void* ctx, Handle main_thread, void* saved_lr) {
	extern char* fake_heap_start;
	extern char* fake_heap_end;

	fake_heap_start = &g_heap[0];
	fake_heap_end   = &g_heap[sizeof g_heap];
	
	orig_ctx = ctx;
	orig_main_thread = main_thread;
	orig_saved_lr = saved_lr;
	
	// Call constructors.
	//void __libc_init_array(void);
	__libc_init_array();
	virtmemSetup();
}

void __attribute__((weak)) NORETURN __libnx_exit(int rc) {
	// Call destructors.
	//void __libc_fini_array(void);
	__libc_fini_array();

	SaltySD_printf("SaltySD Plugin: jumping to %p\n", orig_saved_lr);

	__nx_exit(0, orig_saved_lr);
	while (true);
}

inline uint64_t getMainAddress() {
	MemoryInfo memoryinfo = {0};
	u32 pageinfo = 0;

	uint64_t base_address = SaltySDCore_getCodeStart() + 0x4000;
	for (size_t i = 0; i < 3; i++) {
		Result rc = svcQueryMemory(&memoryinfo, &pageinfo, base_address);
		if (R_FAILED(rc)) return 0;
		if ((memoryinfo.addr == base_address) && (memoryinfo.perm & Perm_X)) {
			main_size = memoryinfo.size;
			return base_address;
		}
		base_address = memoryinfo.addr+memoryinfo.size;
		
	}

	return 0;
}

void WaitEvent(void* EventType) {
	if (*checkFlag_shared) {
		void* instanceAddress = ((GetInstanceOffset)(main_address + instanceOffset))();
		if (U32Flag) {
			string32.command = (char32_t*)string_shared;
			string32.character_count = std::char_traits<char32_t>::length(string32.command) + 1;

			__asm__ __volatile__ (
				"mov x0, %[three];"
				"mov x1, %[four];"
				"mov w2, #0;"
				"mov x8, %[five];"
				"blr %[one];"
				: : [one]"r"(ConsoleCommandAddress), [three]"r"(instanceAddress), [four]"r"(&string32), [five]"r"(&dummy32)
				: "w2", "x1", "x0", "x8"
			);
		}
		else {
			string16.command = string_shared;
			string16.character_count = std::char_traits<char16_t>::length(string16.command) + 1;

			__asm__ __volatile__ (
				"mov x0, %[three];"
				"mov x1, %[four];"
				"mov w2, #0;"
				"mov x8, %[five];"
				"blr %[one];"
				: : [one]"r"(ConsoleCommandAddress), [three]"r"(instanceAddress), [four]"r"(&string16), [five]"r"(&dummy16)
				: "w2", "x1", "x0", "x8"
			);

		}
		*checkFlag_shared = false;
	}
	return _ZN2nn2os9WaitEventEPNS0_9EventTypeE(EventType);
}

bool memcmp_f (const unsigned char *s1, const unsigned char *s2, size_t count) {
    while (count-- > 0) 
        if (*s1++ != *s2++) 
            return false;
    return true;
}

uintptr_t findTextCode(const uint8_t* code, size_t size, uintptr_t addr, size_t addr_size) {
    if (size % 4 != 0) return 0;
    uintptr_t addr_start = addr;
    while (addr != addr_start + addr_size) {
        bool result = memcmp_f((const unsigned char*)addr, code, size);
        if (result) return addr;
        addr += 4;
    }
    return 0;
}

int main(int argc, char *argv[]) {
	SaltySDCore_printf("UE4_CMD %s: alive\n", ver);

	main_address = getMainAddress();
	uint8_t codesearch[12] = {0x00, 0x4C, 0x41, 0xF9, 0x60, 0x00, 0x00, 0xB4, 0x42, 0x00, 0x00, 0x12};
	ConsoleCommandAddress = findTextCode(&codesearch[0], 12, main_address, main_size);

	if (!ConsoleCommandAddress) {
		SaltySDCore_printf("UE4_CMD: CommandLine not found!\n");
		return 0;
	}

	Result ret = SaltySD_CheckIfSharedMemoryAvailable(&SharedMemoryOffset, 261);
	SaltySDCore_printf("UE4_CMD: SharedMemory ret: 0x%X\n", ret);
	if (!ret) {
		SaltySDCore_printf("UE4_CMD: SharedMemory MemoryOffset: %d\n", SharedMemoryOffset);
		SaltySD_GetSharedMemoryHandle(&remoteSharedMemory);
		shmemLoadRemote(&_sharedmemory, remoteSharedMemory, 0x1000, Perm_Rw);
		Result rc = shmemMap(&_sharedmemory);
		if (R_SUCCEEDED(rc)) {
			uintptr_t base = (uintptr_t)shmemGetAddr(&_sharedmemory) + SharedMemoryOffset;
			uint32_t* MAGIC = (uint32_t*)base;
			*MAGIC = 0x584E3455;
			checkFlag_shared = (bool*)(base + 4);
			string_shared = (char16_t*)(base + 5);
			SaltySDCore_ReplaceImport("_ZN2nn2os9WaitEventEPNS0_9EventTypeE", (void*)WaitEvent);

			SaltySDCore_printf("SaltySD UE4_CMD %s: injection finished correctly\n", ver);
		}
		else {
			SaltySDCore_printf("SaltySD UE4_CMD %s: error 0x%X. Couldn't map shmem\n", ver, rc);
		}
	}
	
	SaltySDCore_printf("SaltySD UE4_CMD %s: injection finished\n", ver);
}
