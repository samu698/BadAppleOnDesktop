#pragma once
#include <vector>
#include <chrono>

#include <cstdint>
#include <Windows.h>
#include <CommCtrl.h>
#include <ShlObj.h>

#include "fileGen.h"

using namespace std::chrono_literals;

// this class handles the connection with explorer.exe
// allowing to show the "image"
// the desktop is a ListView and we can control it by sending messages

// screen is 16 by 12 icons
// each icon is subdivided into 4 quadrants
// so we can double the size of the grid making the screen 32 by 24
// and the icons can have 16 possible states (in the code is called type)
class screenManager {

	static BOOL CALLBACK WindowsEnumerator(HWND hwnd, LPARAM param) {
		wchar_t classNameBuf[240];
		GetClassName(hwnd, classNameBuf, 240);
		if (wcscmp(classNameBuf, L"WorkerW") != 0) return TRUE;
		HWND child = GetWindow(hwnd, GW_CHILD);
		if (!child) return TRUE;
		GetClassName(child, classNameBuf, 240);
		if (wcscmp(classNameBuf, L"SHELLDLL_DefView") != 0) return TRUE;
		*(HWND*)param = hwnd;
		return FALSE;
	}
	HWND findSysViewList() {
		// Sometimes the ListView is under a WorkerW window
		// And Sometimes is under Progman, but why?
		HWND MainWnd = NULL;
		EnumWindows(WindowsEnumerator, (LPARAM)&MainWnd);		// if is under a WorkerW
		if (!MainWnd)
			MainWnd = FindWindow(L"Progman", nullptr);			// if is under progman
		return GetWindow(GetWindow(MainWnd, GW_CHILD), GW_CHILD);
	}

	enum bgColors : uint8_t { BLACK = 0, WHITE = 0b1111 };
	// this struct maps an iconId with is type
	struct iconSlot {
		bool inUse;
		uint8_t type;
		uint32_t iconId;
	};

	HWND listView;
	HANDLE explorerHandle;
	void* remoteMem;

	fileGen* fGen;
	
	// list of icons by type
	std::vector<std::vector<iconSlot*>> icons;
	// the icons used in the image
	std::vector<std::vector<iconSlot*>> image;
	// this array contains the state of each pixel
	uint8_t* pixelBuf;

	bgColors bgColor = BLACK;

	using CLOCK = std::chrono::high_resolution_clock;
	using TP = CLOCK::time_point;
	TP lastDesktopColorChange;

	// this function queries the name of an icon from the ListView
	// to obtain the string we need to send a getItem message
	// this message asks explorer.exe to read and write memory
	// so we can't pass local memory because explorer doesn't have access to it
	// to fix this problem we need to place the struct in explorer's memory
	// and then read it back
	std::wstring getIconName(int id) {
		wchar_t localBuf[256];
		wchar_t* remoteBuf = (wchar_t*)remoteMem + 512;
		SIZE_T RW;

		LVITEM queryStruct;
		queryStruct.mask = LVIF_TEXT;
		queryStruct.iItem = id;
		queryStruct.iSubItem = 0;
		queryStruct.pszText = (wchar_t*)remoteMem;
		queryStruct.cchTextMax = 256;

		WriteProcessMemory(explorerHandle, remoteBuf, &queryStruct, sizeof(LVITEM), &RW);
		ListView_GetItem(listView, remoteBuf);
		ReadProcessMemory(explorerHandle, remoteMem, localBuf, sizeof(localBuf), &RW);
		
		return std::wstring(localBuf);
	}

	iconSlot* findAvailableID(uint8_t type) {
		for (auto& slot : icons[type]) {
			if (!slot->inUse) {
				slot->inUse = true;
				return slot;
			}
		}
		std::cerr << "couldn't find slot of type " << (int)type << '\n';
		exit(-100);
		return nullptr;
	}
	void clearID(iconSlot* slot) { 
		slot->inUse = false;
		PostMessage(listView, LVM_SETITEMPOSITION, (WPARAM)slot->iconId, MAKELPARAM(5000, 5000));
	}

	// this function keeps track and updates the desktop color
	// we try to change the desktop color only once a second
	// but because we have a limited number of icons
	// if the white (or black) squares exceed a critical value we change desktop anyway
	// the function return true if the color was changed
	bool setDesktopColor(int whiteSquares) {
		constexpr int threshold = (32 * 24) / 2;
		constexpr int critical = 70; // this value was found by tuning it

		bgColors newColor = (whiteSquares > threshold) ? WHITE : BLACK;
		bool isCritical;
		if (bgColor == WHITE) isCritical = whiteSquares < threshold - critical;
		else if (bgColor == BLACK) isCritical = whiteSquares > threshold + critical;

		std::cout << whiteSquares << '\n';

		if (bgColor == newColor) return false;
		if (!isCritical && CLOCK::now() - lastDesktopColorChange <= 1s) return false;

		fGen->changeDesktop(newColor);
		bgColor = newColor;
		std::cout << "the color should be " << ((newColor == WHITE) ? "white\n" : "black\n");

		// Simulate right click to update desktop color
		POINT mousePos;
		GetCursorPos(&mousePos);
		mouse_event(MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_RIGHTUP, mousePos.x, mousePos.y, 0, 0);

		lastDesktopColorChange = CLOCK::now();
		return true;
	}
public:
	screenManager() : listView(findSysViewList()) {
		DWORD PID;
		GetWindowThreadProcessId(listView, &PID);
		explorerHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, PID);
		remoteMem = VirtualAllocEx(explorerHandle, NULL, 1024 * 1024, MEM_COMMIT, PAGE_READWRITE);

		wchar_t pathBuf[MAX_PATH];
		SHGetFolderPath(NULL, CSIDL_DESKTOP, NULL, 0, pathBuf);
		std::wstring desktopPath(pathBuf);
		SHGetFolderPath(NULL, CSIDL_MYDOCUMENTS, NULL, 0, pathBuf);
		fGen = new fileGen(desktopPath, pathBuf);
		fGen->generateFiles();

		// allocate buffers
		for (int i = 0; i < 16; i++) {
			icons.emplace_back(); 
			icons[i].reserve(96 + 6);
		}
		for (int i = 0; i < 16; i++) {
			image.emplace_back();
			image[i].reserve(12);
			for (int j = 0; j < 12; j++) image[i].push_back(nullptr);
		}
		pixelBuf = new uint8_t[16 * 12]();
		Sleep(5000);
	}

	// this function checks the name of each file in the ListView
	// and assigns to each icon the right type using the name
	// finally it put the icon offscreen so it won't lag explorer
	void createIconBuffers() {
		uint32_t iconCount = ListView_GetItemCount(listView);
		for (uint32_t i = 0; i < iconCount; i++) {
			ListView_SetItemPosition(listView, i, 5000, 5000);
			std::wstring name = getIconName(i);
			uint8_t type = fGen->getFileType(name);
			if (type == 0xFF) continue;
			iconSlot* slot = new iconSlot({ false, type, i });
			icons[type].push_back(slot);
		}
	}

	void SetStartingDesktopColor(bool* pixelGrid) {
		int whiteSquares = 0;
		constexpr int threshold = (32 * 24) / 2;

		for (uint32_t y = 0; y < 24; y++)
			for (uint32_t x = 0; x < 32; x++)
				if (pixelGrid[y * 32 + x]) whiteSquares++;
		
		bgColors color = (whiteSquares > threshold) ? WHITE : BLACK;

		fGen->changeDesktop(color);
		bgColor = color;
		lastDesktopColorChange = CLOCK::now();
	}

	// this is were the magic happends
	void RenderImage(bool* pixelGrid) {
		constexpr int ImgSize = 29;
		constexpr int Cx = 1920 / 2, Bx = Cx - ImgSize * 16 / 2;
		constexpr int Cy = 1080 / 2, By = Cy - ImgSize * 12 / 2;

		int whiteSquares = 0;

		memset(pixelBuf, 0, 16 * 12);
		for (uint32_t y = 0; y < 24; y++) {
			uint32_t pbY = y / 2;
			uint8_t typeOff = y % 2;
			for (uint32_t x = 0; x < 32; x++) {
				bool val = pixelGrid[y * 32 + x];
				if (val) whiteSquares++;
				uint32_t pbX = x / 2;
				pixelBuf[pbY * 16 + pbX] |= val << typeOff + (x % 2) * 2;
			}
		}

		bool desktopChanged = setDesktopColor(whiteSquares);

		for (uint32_t y = 0; y < 12; y++) {
			for (uint32_t x = 0; x < 16; x++) {
				uint8_t type = pixelBuf[y * 16 + x];

				iconSlot* slotInPos = image[x][y];
				if (!slotInPos && type != bgColor) {						// the empty slot gets filled
					iconSlot* slot = findAvailableID(type);
					image[x][y] = slot;
					int iconX = Bx + ImgSize * x;
					int iconY = By + ImgSize * y;
					PostMessage(listView, LVM_SETITEMPOSITION, (WPARAM)slot->iconId, MAKELPARAM(iconX, iconY));
				}
				if (!slotInPos) continue;									// the slot remains empty

				if (desktopChanged && slotInPos->type == bgColor) {			// when changing desktop remove the old icons
					clearID(slotInPos);										// that have the same color as the background
					image[x][y] = nullptr;
					continue;
				}

				if (slotInPos->type == type) continue;						// the slot doesn't change

				clearID(slotInPos);											// the slot has changed so release the current icon used

				if (type == bgColor) {										// if the new icon is empty then clear
					image[x][y] = nullptr;
					continue;
				}

				iconSlot* slot = findAvailableID(type);
				image[x][y] = slot;
				int iconX = Bx + ImgSize * x;
				int iconY = By + ImgSize * y;
				PostMessage(listView, LVM_SETITEMPOSITION, (WPARAM)slot->iconId, MAKELPARAM(iconX, iconY));
			}
		}
	}

	~screenManager() {
		VirtualFreeEx(explorerHandle, remoteMem, 0, MEM_RELEASE);
		CloseHandle(explorerHandle);
		delete fGen;
		delete[] pixelBuf;
	}
};