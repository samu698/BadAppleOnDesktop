#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <cstdint>

#include "screenManager.h"

// Well i wanted to use openCV, but after 3 days
// of tring to making it work I gave up.
// for some reason even if I used sample code
// I was getting a read access violation
// so this is my solution:
// 1. converting the video to an image seqence
// 2. using stb_image to read the data (because is easyer than using libjpeg)
// 3. using CImg to resize and display the image
#include "CImg.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// ############## IMPORTANT #################
// edit this to the path of your bad apple image sequence
// the file names should end with <n>.jpg were n starts from 1
// to get the image seqence with this names you can use the command
// ffmpeg -i badApple.mp4 C:\Path\To\File\file%d.jpg
#define IMG_SEQ_PATH "C:\\Path\\To\\File\\file"

using namespace cimg_library;
using namespace std::chrono_literals;
// simple class to keep everything in sync
// this uses a busy loop, whitch is not great
// if you have a better way you can do a pull request
class FrameClock {
	using CLOCK = std::chrono::high_resolution_clock;
	using TP = CLOCK::time_point;

	const float targetFPS;
	const uint64_t targetDeltaNs;

	const TP startTime;
	TP lastFrameTP;
	int frameCount = 0;
public:
	FrameClock(float targetFPS = 30.0f) : targetFPS(targetFPS), targetDeltaNs(1e9 / targetFPS), startTime(CLOCK::now()) {
		lastFrameTP = startTime;
	}

	int getFrame() { return frameCount; }

	void WaitNextFrame() {
		frameCount++;
		TP now = CLOCK::now();
		uint64_t currentDelta = (now - lastFrameTP).count();
		while (currentDelta < targetDeltaNs) {
			if (currentDelta < (targetDeltaNs * 0.5)) Sleep(1); // Don't hog the CPU always
			now = CLOCK::now();
			currentDelta = (now - lastFrameTP).count();
		}
		lastFrameTP = now;
	}
	
	~FrameClock() {
		TP endTime = CLOCK::now();
		double timeMs = (endTime - startTime).count() / 1e6;
		std::cout << "displayed " << frameCount << " in " << timeMs << "ms\n";

		double FPS = frameCount / (timeMs / 1000.0);
		double TimePerFrame = timeMs / frameCount;

		std::cout << "At a speed of " << FPS << "FPS (" << TimePerFrame << "ms per frame)\n";
	}
};

int main() {

	CImgDisplay window(480, 360, "display");
	uint8_t* imgData = nullptr;
	CImg<uint8_t> frame(480, 360, 3);
	CImg<uint8_t> resized(32, 24, 3);

	screenManager screenMan;
	screenMan.createIconBuffers();
	bool* pixelBuf = new bool[32 * 24]();
	screenMan.SetStartingDesktopColor(pixelBuf);

	FrameClock clock;
	while (!window.is_closed()) {
		int x, y, ch;

		std::string img = IMG_SEQ_PATH;						// this is the path to the image sequence
		img += std::to_string(clock.getFrame() + 1);		// in my case the file name is ba%d.jpg where %d is a counter starting from 1
		img += ".jpg";

		if (imgData) stbi_image_free(imgData);
		imgData = stbi_load(img.c_str(), &x, &y, &ch, 3);

		if (!imgData) break;								// no image loaded, reached the end

		frame.assign(imgData, 3, x, y, 1);					// little trick for using stbi_image data
		frame.permute_axes("yzcx");

		window.display(frame);
		
		resized = frame.resize(32, 24, 1, 1, 3);

		for (int cy = 0; cy < 24; cy++)
			for (int cx = 0; cx < 32; cx++)
				pixelBuf[cy * 32 + cx] = resized(cx, cy) > 127;

		screenMan.RenderImage(pixelBuf);

		clock.WaitNextFrame();
	}
}
