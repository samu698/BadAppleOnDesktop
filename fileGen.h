#pragma once
#include <string>
#include <filesystem>
#include "bmp.h"

// this class is used to generate an keep track of the files used
// for the image we need 16 types of image each in 102 copies
// each image has an invisible name using the chars C0 and C1
// to make the name truly invisible we need to disable extensions
// here is the format of the file name
// BIT:			  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
// USAGE:		  T  T  T  T  C  C  C  C  C  C  C  X  X  X  X  X
// T: img type C:counter X:unused

// also this class handles the desktop background images

#define GETBIT(n, b) ((n >> b) & 1)

class fileGen {
	const wchar_t C0 = L'\x200B';
	const wchar_t C1 = L'\xFEFF';

	bool* pixelBuffer;
	std::wstring desktopPath;
	std::wstring documentPath;

	std::wstring blackDesktopImg;
	std::wstring whiteDesktopImg;

	void genPixelData(uint8_t type) {
		for (int32_t y = 0; y < 29; y++) {
			for (int32_t x = 0; x < 29; x++) {
				uint8_t quadrant = 0;
				if (x > 14) quadrant++;
				if (y > 14) quadrant += 2;
				pixelBuffer[y * 29 + x] = GETBIT(type, quadrant);
			}
		}
	}
	std::wstring genFileName(uint8_t type, uint8_t counter) {
		std::wstring result = desktopPath + L"\\";
		for (uint8_t i = 0; i < 4; i++) result += GETBIT(type, i) ? C1 : C0;
		for (uint8_t i = 0; i < 7; i++) result += GETBIT(counter, i) ? C1 : C0;
		result += L".bmp";
		return result;
	}
public:
	fileGen(std::wstring desktopPath, std::wstring documentPath) : pixelBuffer((bool*)malloc(29 * 29)), desktopPath(desktopPath) {
		blackDesktopImg = documentPath + L"\\BADBlackDesktopImg.bmp";
		whiteDesktopImg = documentPath + L"\\BADWhiteDesktopImg.bmp";
	}
	void generateFiles() {
		for (uint8_t type = 0; type < 16; type++) {
			genPixelData(type);
			for (int32_t counter = 0; counter < 96 + 6; counter++) {
				BMPWriter::writeBMP(29, 29, pixelBuffer, genFileName(type, counter));
			}
		}

		bool pixel[1];
		pixel[0] = 0;
		BMPWriter::writeBMP(1, 1, pixel, blackDesktopImg);
		pixel[0] = 1;
		BMPWriter::writeBMP(1, 1, pixel, whiteDesktopImg);
	}
	void changeDesktop(bool color) {
		SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, (void*)(color ? whiteDesktopImg.c_str() : blackDesktopImg.c_str()), SPIF_UPDATEINIFILE);
	}
	uint8_t getFileType(std::wstring fileName) {
		uint8_t result = 0;
		uint8_t pos = 0;
		if (fileName[0] != C0 && fileName[0] != C1) return 0xFF;
		for (wchar_t c : fileName) {
			result |= (c == C1) << pos++;
			if (pos == 4) break;
		}
		return result;
	}
	~fileGen() {
		free(pixelBuffer);
	}
};