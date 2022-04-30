#include "pch.h"
#include "CoachX.h"

// Plugin Settings Window code here
std::string CoachX::GetPluginName() {
	return "CoachX";
}

// render the plugin settings here
// This will show up in bakkesmod when the plugin is loaded at
//  f2 -> plugins -> CoachX
void CoachX::RenderSettings() {
	ImGui::TextUnformatted("CoachX plugin settings");
	if (ImGui::Button("Open Settings Window")) {
		game_wrapper->Execute([this](GameWrapper* game_wrapper) {
			cvar_manager->executeCommand("togglemenu CoachX");
		});
	}
	ImGui::TextUnformatted(R"(Tip: You can bind "togglemenu CoachX" to any key under "F2 -> Bindings")");
}



// Do ImGui rendering here
void CoachX::Render() {
	if (!ImGui::Begin(menu_title_.c_str(), &is_window_open_, ImGuiWindowFlags_None)) {
		// Early out if the window is collapsed, as an optimization.
		ImGui::End();
		return;
	}

	if (ImGui::Checkbox("Kickoff Timer", &cfg_.kickoff_timer_enabled)) {
		if (cfg_.kickoff_timer_enabled) {
			kickoffTimer->enable();
		} else {
			kickoffTimer->disable();
		}
	}
	if (cfg_.kickoff_timer_enabled) {
		kickoffTimer->render_settings();
	}

	ImGui::End();

	if (!is_window_open_) {
		cvarManager->executeCommand("togglemenu " + GetMenuName());
	}
}

// Name of the menu that is used to toggle the window.
std::string CoachX::GetMenuName()
{
	return "CoachX";
}

// Title to give the menu
std::string CoachX::GetMenuTitle()
{
	return menu_title_;
}

// Don't call this yourself, BM will call this function with a pointer to the current ImGui context
void CoachX::SetImGuiContext(uintptr_t ctx)
{
	ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

// Should events such as mouse clicks/key inputs be blocked so they won't reach the game
bool CoachX::ShouldBlockInput()
{
	return ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard;
}

// Return true if window should be interactive
bool CoachX::IsActiveOverlay()
{
	return true;
}

// Called when window is opened
void CoachX::OnOpen()
{
	is_window_open_ = true;
}

// Called when window is closed
void CoachX::OnClose()
{
	is_window_open_ = false;
}

