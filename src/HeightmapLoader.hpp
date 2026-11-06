#pragma once
#include <SDL3/SDL.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <tiffio.h>

#include <execution>
#include <vector>

namespace LoadFile {
static std::vector<float> Tiff(const char* filepath) {
	// Loading Tiff took 2518 ms - Original
	// Loading Tiff took 2284 ms - Writing row instead of pixels
	// Loading Tiff took 891 ms - multithreaded
	int w, h;
	std::vector<float> map;
	TIFF* tif = TIFFOpen(filepath, "r");
	if (!tif) SDL_Log("Failed to load tiff file", SDL_GetError());

	uint32_t tileWidth, tileHeight;
	TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
	TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
	TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tileWidth);
	TIFFGetField(tif, TIFFTAG_TILELENGTH, &tileHeight);
	if (!TIFFIsTiled(tif)) {
		SDL_Log("Not tiled");
		TIFFClose(tif);
		return map;
	}
	TIFFClose(tif);	 // Close file for multithread opening

	map.resize(w * h);

	std::vector<std::pair<uint32_t, uint32_t>> tilePositions;
	for (uint32_t y = 0; y < h; y += tileHeight) {
		for (uint32_t x = 0; x < w; x += tileWidth) {
			tilePositions.push_back({ x, y });
		}
	}

	// Copy tiff file to std::vector<float> (multithreaded)

	std::for_each(std::execution::par, tilePositions.begin(), tilePositions.end(), [&](const auto& pos) {
		uint32_t tile_start_x = pos.first;
		uint32_t tile_start_y = pos.second;

		TIFF* thread_tif = TIFFOpen(filepath, "r");
		if (!thread_tif) return;

		tmsize_t tileSize = TIFFTileSize(thread_tif);
		std::vector<float> tileBuffer(tileSize / sizeof(float));

		uint32_t validHeight = std::min(tileHeight, h - tile_start_y);
		uint32_t validWidth = std::min(tileWidth, w - tile_start_x);

		TIFFReadTile(thread_tif, tileBuffer.data(), tile_start_x, tile_start_y, 0, 0);

		for (uint32_t tileY = 0; tileY < validHeight; tileY++) {
			uint32_t dest_row_idx = (tile_start_y + tileY) * w + tile_start_x;
			uint32_t src_row_idx = tileY * tileWidth;
			std::copy_n(tileBuffer.begin() + src_row_idx, validWidth, map.begin() + dest_row_idx);
		}
	});
	return map;
}
static std::vector<float> PNG(const char* filepath) {
	int width, height, channels;
	float* data = stbi_loadf(filepath, &width, &height, &channels, 1);
	std::vector<float> img(data, data + (width * height));

	stbi_image_free(data);
	return {};
}
}  // namespace LoadFile