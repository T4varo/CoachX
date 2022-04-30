#pragma once

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"

#include "version.h"
#include "KickoffTimer.h"
#include "MultipleHookException.h"
constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

class CoachX: public BakkesMod::Plugin::BakkesModPlugin, public BakkesMod::Plugin::PluginWindow, public BakkesMod::Plugin::PluginSettingsWindow
{
public:
	virtual void onLoad() override;
	virtual void onUnload() override;

	void render_canvas(CanvasWrapper canvas) const;

	// Plugin Window
	void Render() override;
	std::string GetMenuName() override;
	std::string GetMenuTitle() override;
	void SetImGuiContext(uintptr_t ctx) override;
	bool ShouldBlockInput() override;
	bool IsActiveOverlay() override;
	void OnOpen() override;
	void OnClose() override;

	// Plugin Settings Window
	std::string GetPluginName() override;
	void RenderSettings() override;

	// Custom hook logic to detect multiple hooks to the same event
	void HookEvent(const std::string &event_name, const std::function<void(const std::string &event_name)> &callback) {
		if (!is_hooked(event_name)) {
			hooked_events_.push_back(event_name);
			gameWrapper->HookEvent(event_name, callback);
		} else {
			throw MultipleHookException(event_name);
		}
	}
	void UnhookEvent(const std::string &event_name) {
		gameWrapper->UnhookEvent(event_name);
		unhook(event_name);
	}
	void HookEventPost(const std::string &event_name, const std::function<void(const std::string &event_name)> &callback) {
		if (!is_hooked_post(event_name)) {
			hooked_events_post_.push_back(event_name);
			gameWrapper->HookEventPost(event_name, callback);
		} else {
			throw MultipleHookException("post " + event_name);
		}
	}
	void UnhookEventPost(const std::string &event_name) {
		gameWrapper->UnhookEventPost(event_name);
		unhook_post(event_name);
	}

	template<typename T, typename std::enable_if<std::is_base_of<ObjectWrapper, T>::value>::type* = nullptr>
	void HookEventWithCallerPost(std::string event_name, std::function<void(T caller, void* params, const std::string event_name)> callback) {
		if (!is_hooked_post(event_name)) {
			hooked_events_post_.push_back(event_name);
			gameWrapper->HookEventWithCallerPost(event_name, callback);
		} else {
			throw MultipleHookException("with caller post " + event_name);
		}
	}
	template<typename T, typename std::enable_if<std::is_base_of<ObjectWrapper, T>::value>::type* = nullptr>
	void HookEventWithCaller(const std::string &event_name, std::function<void(T caller, void* params, const std::string event_name)> callback) {
		if (!is_hooked(event_name)) {
			hooked_events_.push_back(event_name);
			gameWrapper->HookEventWithCaller(event_name, callback);
		} else {
			throw MultipleHookException("with caller " + event_name);
		}
	}
private:
	std::unique_ptr<KickoffTimer> kickoffTimer;

	// Plugin window
	const std::string menu_title_ = "CoachX";
	bool is_window_open_ = false;

	struct config {
		// Features
		bool kickoff_timer_enabled = true;
	};
	config cfg_{};

	void handle_tick(ControllerInput* input);

	// Custom hook helper
	std::vector<std::string> hooked_events_;
	std::vector<std::string> hooked_events_post_;

	bool is_hooked(const std::string &event_name) {
		if (std::find(hooked_events_.begin(), hooked_events_.end(), event_name) == hooked_events_.end()) {
			return false;
		} else {
			return true;
		}
	}
	bool is_hooked_post(const std::string &event_name) {
		if (std::find(hooked_events_post_.begin(), hooked_events_post_.end(), event_name) == hooked_events_post_.end()) {
			return false;
		} else {
			return true;
		}
	}
	void unhook(const std::string &event_name) {
		auto it = std::find(hooked_events_.begin(), hooked_events_.end(), event_name);
		if (it != hooked_events_.end()) {
			hooked_events_.erase(it);
		}
	}
	void unhook_post(const std::string &event_name) {
		auto it = std::find(hooked_events_post_.begin(), hooked_events_post_.end(), event_name);
		if (it != hooked_events_post_.end()) {
			hooked_events_post_.erase(it);
		}
	}
};

