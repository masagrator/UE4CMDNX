// Include the main libnx system header, for Switch development
#include <switch.h>
#include "SaltyNX.h"
#include <cstdio>
#include <cstring>

// See also libnx swkbd.h.

// This example shows how to use the software keyboard (swkbd) LibraryApplet.

uint64_t PID = 0;
Handle remoteSharedMemory = 1;
SharedMemory _sharedmemory = {};
bool SharedMemoryUsed = false;
bool check = false;

bool LoadSharedMemory() {
	if (SaltySD_Connect())
		return false;

	SaltySD_GetSharedMemoryHandle(&remoteSharedMemory);
	SaltySD_Term();

	shmemLoadRemote(&_sharedmemory, remoteSharedMemory, 0x1000, Perm_Rw);
	if (!shmemMap(&_sharedmemory)) {
		SharedMemoryUsed = true;
		return true;
	}
	return false;
}

ptrdiff_t searchSharedMemoryBlock(uintptr_t base) {
	ptrdiff_t search_offset = 0;
	while(search_offset < 0x1000) {
		uint32_t* MAGIC_shared = (uint32_t*)(base + search_offset);
		if (*MAGIC_shared == 0x584E3455) {
			return search_offset;
		}
		else search_offset += 4;
	}
	return -1;
}

bool CheckPort () {
	Handle saltysd;
	for (int i = 0; i < 67; i++) {
		if (R_SUCCEEDED(svcConnectToNamedPort(&saltysd, "InjectServ"))) {
			svcCloseHandle(saltysd);
			break;
		}
		else {
			if (i == 66) return false;
			svcSleepThread(1'000'000);
		}
	}
	for (int i = 0; i < 67; i++) {
		if (R_SUCCEEDED(svcConnectToNamedPort(&saltysd, "InjectServ"))) {
			svcCloseHandle(saltysd);
			return true;
		}
		else svcSleepThread(1'000'000);
	}
	return false;
}

// TextCheck callback, this can be removed when not using TextCheck.
SwkbdTextCheckResult validate_text(char* tmp_string, size_t tmp_string_size) {
	if (strcmp(tmp_string, "bad")==0) {
		strncpy(tmp_string, "Bad string.", tmp_string_size);
		return SwkbdTextCheckResult_Bad;
	}

	return SwkbdTextCheckResult_OK;
}

// The rest of these callbacks are for swkbd-inline.
void finishinit_cb(void) {
	printf("reply: FinishedInitialize\n");
}

void decidedcancel_cb(void) {
	printf("reply: DecidedCancel\n");
}

// String changed callback.
void strchange_cb(const char* str, SwkbdChangedStringArg* arg) {
	printf("reply: ChangedString. str = %s, arg->stringLen = 0x%x, arg->dicStartCursorPos = 0x%x, arg->dicEndCursorPos = 0x%x, arg->arg->cursorPos = 0x%x\n", str, arg->stringLen, arg->dicStartCursorPos, arg->dicEndCursorPos, arg->cursorPos);
}

// Moved cursor callback.
void movedcursor_cb(const char* str, SwkbdMovedCursorArg* arg) {
	printf("reply: MovedCursor. str = %s, arg->stringLen = 0x%x, arg->cursorPos = 0x%x\n", str, arg->stringLen, arg->cursorPos);
}

// Text submitted callback, this is used to get the output text from submit.
void decidedenter_cb(const char* str, SwkbdDecidedEnterArg* arg) {
	printf("reply: DecidedEnter. str = %s, arg->stringLen = 0x%x\n", str, arg->stringLen);
}

// Main program entrypoint
int main(int argc, char* argv[])
{
	Result rc=0;
	pmdmntInitialize();

	// This example uses a text console, as a simple way to output text to the screen.
	// If you want to write a software-rendered graphics application,
	//   take a look at the graphics/simplegfx example, which uses the libnx gfx API instead.
	// If on the other hand you want to write an OpenGL based application,
	//   take a look at the graphics/opengl set of examples, which uses EGL instead.

	consoleInit(NULL);

	bool SaltySD = CheckPort();
	if (!SaltySD) {
		printf("SaltySD port check failed!");
		consoleUpdate(NULL);
		while (appletMainLoop()) {}
		consoleExit(NULL);
		return 0;
	}

	if (R_FAILED(pmdmntGetApplicationProcessId(&PID))) {
		printf("Game is not running!");
		consoleUpdate(NULL);
		while (appletMainLoop()) {}
		consoleExit(NULL);
		return 0;
	}
	check = true;
	
	if(!LoadSharedMemory()) {
		printf("LoadSharedMemory failed!");
		consoleUpdate(NULL);
		while (appletMainLoop()) {}
		consoleExit(NULL);
		return 0;
	}

	bool PluginRunning = false;
	bool* checkflag = 0;
	char* command = 0;

	if (!PluginRunning) {
		uintptr_t base = (uintptr_t)shmemGetAddr(&_sharedmemory);
		ptrdiff_t rel_offset = searchSharedMemoryBlock(base);
		if (rel_offset > -1) {
			checkflag = (bool*)(base + rel_offset + 4);
			command = (char*)(base + rel_offset + 5);
			PluginRunning = true;
		}		
	}

	// Configure our supported input layout: a single player with standard controller styles
	padConfigureInput(1, HidNpadStyleSet_NpadStandard);

	// Initialize the default gamepad (which reads handheld mode inputs as well as the first connected controller)
	PadState pad;
	padInitializeDefault(&pad);

	printf("swkbd example\n");

	consoleUpdate(NULL);

	if (PluginRunning && !*checkflag) {
		SwkbdConfig kbd;
		char tmpoutstr[129] = {0};
		rc = swkbdCreate(&kbd, 0);
		printf("swkbdCreate(): 0x%x\n", rc);

		if (R_SUCCEEDED(rc)) {
			// Select a Preset to use, if any.
			swkbdConfigMakePresetDefault(&kbd);
			//swkbdConfigMakePresetPassword(&kbd);
			//swkbdConfigMakePresetUserName(&kbd);
			//swkbdConfigMakePresetDownloadCode(&kbd);

			// Optional, set any text if you want (see swkbd.h).
			//swkbdConfigSetOkButtonText(&kbd, "Submit");
			//swkbdConfigSetLeftOptionalSymbolKey(&kbd, "a");
			//swkbdConfigSetRightOptionalSymbolKey(&kbd, "b");
			//swkbdConfigSetHeaderText(&kbd, "Header");
			//swkbdConfigSetSubText(&kbd, "Sub");
			//swkbdConfigSetGuideText(&kbd, "Guide");

			swkbdConfigSetTextCheckCallback(&kbd, validate_text);//Optional, can be removed if not using TextCheck.
			swkbdConfigSetStringLenMax(&kbd, 128);
			swkbdConfigSetStringLenMin(&kbd, 2);

			// Set the initial string if you want.
			//swkbdConfigSetInitialText(&kbd, "Initial");

			// You can also use swkbdConfigSet*() funcs if you want.

			printf("Running swkbdShow...\n");
			rc = swkbdShow(&kbd, tmpoutstr, sizeof(tmpoutstr));
			printf("swkbdShow(): 0x%x\n", rc);

			if (R_SUCCEEDED(rc)) {
				printf("out str: %s\n", tmpoutstr);
				size_t character_count = strlen(tmpoutstr) + 1;
				for (size_t i = 0; i < character_count; i++) {
					command[i*2] = tmpoutstr[i];
					command[(i*2)+1] = 0;
				}
				*checkflag = true;
			}
			swkbdClose(&kbd);
		}
	}
	else {
		if (!PluginRunning) printf("Plugin is not running...\n");
		else if (*checkflag) printf("Command was not parsed yet...\n");
	}

	while (appletMainLoop())
	{
		// Scan the gamepad. This should be done once for each frame
		padUpdate(&pad);

		// padGetButtonsDown returns the set of buttons that have been
		// newly pressed in this frame compared to the previous one
		u64 kDown = padGetButtonsDown(&pad);

		if (kDown & HidNpadButton_Plus)
			break; // break in order to return to hbmenu

		// Update the console, sending a new frame to the display
		consoleUpdate(NULL);
	}


	// Deinitialize and clean up resources used by the console (important!)
	consoleExit(NULL);
	return 0;
}
