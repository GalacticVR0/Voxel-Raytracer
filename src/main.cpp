#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_shadercross/SDL_shadercross.h>

#include <unordered_map>

#include "VoxelHandler.hpp"
#include "VoxelRenderer.hpp"
#include "utils.hpp"

struct AppState {
	SDL_Window* window;
	VoxelRenderer renderer;

	std::unordered_map<SDL_Scancode, bool> keys;
	Camera camera;

	Uint64 lastTick;

	AppState() : lastTick(SDL_GetTicksNS()), camera({ 0, 0, -6 }, 0, 0) {}
};

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
	*appstate = new AppState();
	AppState& app = *static_cast<AppState*>(*appstate);

	if (!SDL_Init(SDL_INIT_VIDEO)) {
		SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}
	if (!SDL_ShaderCross_Init()) {
		SDL_Log("Failed to initialize SDL_ShaderCross: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	app.window = SDL_CreateWindow("VoxelRaytracer", 1280, 720, SDL_WINDOW_RESIZABLE);
	if (!app.window) {
		SDL_Log("Failed to create window: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}
	SDL_SetWindowRelativeMouseMode(app.window, true);
	if (!app.renderer.Initialize(app.window, 1280, 720)) {
		return SDL_APP_FAILURE;
	}
	// Build chunk
	World world({ 1, 1, 1 });
	for (int z = 0; z < 10; ++z) {
		for (int x = 0; x < 10; ++x) {
			uint32_t color = 0xFF000000 | ((x * 25) << 16) | ((z * 25) << 8);
			world.setVoxel({ x, 0, z }, Chunking::Voxel{ color });
		}
	}
	// Upload octree to gpu
	app.renderer.submitOctree(world.buildOctree());
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
	AppState& app = *static_cast<AppState*>(appstate);
	if (event->type == SDL_EVENT_QUIT) {
		return SDL_APP_SUCCESS;
	} else if (event->type == SDL_EVENT_KEY_DOWN) {
		if (event->key.scancode == SDL_SCANCODE_W) app.keys[SDL_SCANCODE_W] = true;
		if (event->key.scancode == SDL_SCANCODE_S) app.keys[SDL_SCANCODE_S] = true;

		if (event->key.scancode == SDL_SCANCODE_A) app.keys[SDL_SCANCODE_A] = true;
		if (event->key.scancode == SDL_SCANCODE_D) app.keys[SDL_SCANCODE_D] = true;

		if (event->key.scancode == SDL_SCANCODE_E) app.keys[SDL_SCANCODE_E] = true;
		if (event->key.scancode == SDL_SCANCODE_Q) app.keys[SDL_SCANCODE_Q] = true;
	} else if (event->type == SDL_EVENT_KEY_UP) {
		if (event->key.scancode == SDL_SCANCODE_W) app.keys[SDL_SCANCODE_W] = false;
		if (event->key.scancode == SDL_SCANCODE_S) app.keys[SDL_SCANCODE_S] = false;

		if (event->key.scancode == SDL_SCANCODE_A) app.keys[SDL_SCANCODE_A] = false;
		if (event->key.scancode == SDL_SCANCODE_D) app.keys[SDL_SCANCODE_D] = false;

		if (event->key.scancode == SDL_SCANCODE_E) app.keys[SDL_SCANCODE_E] = false;
		if (event->key.scancode == SDL_SCANCODE_Q) app.keys[SDL_SCANCODE_Q] = false;
	} else if (event->type == SDL_EVENT_MOUSE_MOTION) {
		app.camera.rotate(event->motion.xrel * 0.01, -event->motion.yrel * 0.01);
	}

	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
	AppState& app = *static_cast<AppState*>(appstate);
	Uint64 currentTick = SDL_GetTicksNS();
	float dt = float(currentTick - app.lastTick) / 1'000'000'000;
	app.lastTick = currentTick;

	float speed = 10 * dt;

	if (app.keys.contains(SDL_SCANCODE_W) && app.keys[SDL_SCANCODE_W]) {
		app.camera.move({ app.camera.gpuData.forward * speed });
	}
	if (app.keys.contains(SDL_SCANCODE_S) && app.keys[SDL_SCANCODE_S]) {
		app.camera.move({ app.camera.gpuData.forward * -speed });
	}
	if (app.keys.contains(SDL_SCANCODE_A) && app.keys[SDL_SCANCODE_A]) {
		app.camera.move({ app.camera.gpuData.right * -speed });
	}
	if (app.keys.contains(SDL_SCANCODE_D) && app.keys[SDL_SCANCODE_D]) {
		app.camera.move({ app.camera.gpuData.right * speed });
	}
	if (app.keys.contains(SDL_SCANCODE_E) && app.keys[SDL_SCANCODE_E]) {
		app.camera.move({ app.camera.gpuData.up * speed });
	}
	if (app.keys.contains(SDL_SCANCODE_Q) && app.keys[SDL_SCANCODE_Q]) {
		app.camera.move({ app.camera.gpuData.up * -speed });
	}

	app.renderer.DrawFrame(app.window, app.camera.gpuData);

	return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
	if (!appstate) {
		return;
	}
	AppState& app = *static_cast<AppState*>(appstate);

	if (app.window) SDL_DestroyWindow(app.window);

	delete &app;
	return;
}
