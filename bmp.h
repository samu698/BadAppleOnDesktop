#include <fstream>
#include <iomanip>
#include <string>
#include <cstdint>

// little BMP writer class that creates B/W bitmaps
class BMPWriter {
	#pragma pack(1)
	struct BMPHeader {
		char			magic[2] = { 'B', 'M' };
		uint32_t		fileSize;
		uint32_t		reserved = 0;
		uint32_t		PixelArrayOffset = 122;
	};
	#pragma pack(1)
	struct DIBV4Header {
		uint32_t		DIBSize = 108;
		int32_t			width;
		int32_t			height;
		int16_t			planes = 1;
		int16_t			bpp = 16;
		uint32_t		compression = 3;
		uint32_t		sizeImage;
		int32_t			XPelsPerMeter = 2835;
		int32_t			YPelsPerMeter = 2835;
		uint32_t		clrUsed = 0;
		uint32_t		clrImportant = 0;
		uint32_t		redMask = 0x1F;
		uint32_t		greenMask = 0x7E0;
		uint32_t		blueMask = 0xF800;
		uint32_t		alphaMask = 0x00;
		uint32_t		CSType = 0x206E6957;
		uint8_t			unused[0x24] = {0};
		uint32_t		gammaRed = 0;
		uint32_t		gammaGreen = 0;
		uint32_t		gammaBlue = 0;
	};
public:
	static void writeBMP(int32_t w, int32_t h, bool* pixels, std::wstring FileName) {
		std::ofstream output(FileName, std::ios_base::binary);
		BMPHeader header;
		DIBV4Header header2;
		
		uint32_t paddedH = h + h % 2;

		header.fileSize = header.PixelArrayOffset + w * paddedH * 2;
		header2.width = w;
		header2.height = h;
		header2.sizeImage = w * paddedH * 2;

		for (uint32_t i = 0; i < sizeof(header); i++) output << ((char*)&header)[i];
		for (uint32_t i = 0; i < sizeof(header2); i++) output << ((char*)&header2)[i];

		for (int32_t x = w - 1; x >= 0; x--) {
			for (int32_t y = 0; y < paddedH; y++) {
				if (y >= h) {
					output << (uint8_t)0x00;
					output << (uint8_t)0x00;
				}
				else {
					output << (uint8_t)(pixels[y * w + x] ? 0xFF : 0x00);
					output << (uint8_t)(pixels[y * w + x] ? 0xFF : 0x00);
				}
			}
		}

		output.close();
	}
};