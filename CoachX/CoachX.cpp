#include "pch.h"
#include "CoachX.h"


BAKKESMOD_PLUGIN(CoachX, "Tools to improve your mechanics", plugin_version, PLUGINTYPE_FREEPLAY)

// Global Wrapper Pointer
std::shared_ptr<CVarManagerWrapper> cvar_manager;
std::shared_ptr<GameWrapper> game_wrapper;
//TODO: std::shared_ptr<CoachX> _globalCoachXWrapper;
CoachX* coach_x_wrapper;

void CoachX::onLoad()
{
	cvar_manager = cvarManager;
	game_wrapper = gameWrapper;
	coach_x_wrapper = this;

	kickoffTimer = std::make_unique<KickoffTimer>();
	if (cfg_.kickoff_timer_enabled) kickoffTimer->enable();

	// render_canvas Overlay
	game_wrapper->RegisterDrawable([this](CanvasWrapper canvas) {
		render_canvas(canvas);
	});

	coach_x_wrapper->HookEventWithCaller<CarWrapper>("Function TAGame.Car_TA.SetVehicleInput", [this](const CarWrapper& car, void* params, const std::string& event_name) {
		handle_tick(static_cast<ControllerInput*>(params));
	});

	/*
	coach_x_wrapper->HookEventPost("Function Engine.GameViewportClient.Tick", [this](const std::string& event_name) {
		if (!is_window_open_) {
			LOG("Opening window...");
			cvar_manager->executeCommand("togglemenu CoachX");
			coach_x_wrapper->UnhookEventPost(event_name);
		}
	});
	*/
	LOG("Plugin CoachX enabled!");
}

void CoachX::onUnload()
{
	kickoffTimer->disable();

	LOG("Plugin CoachX disabled!");
}

void CoachX::render_canvas(CanvasWrapper canvas) const {
	if (cfg_.kickoff_timer_enabled)	kickoffTimer->render_canvas(&canvas);
}

void CoachX::handle_tick(ControllerInput* input) {
	// Pass input to features
	if (kickoffTimer->is_active()) kickoffTimer->handle_tick(input);
}