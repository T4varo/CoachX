#pragma once

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include "bakkesmod/plugin/bakkesmodplugin.h"

#include <string>
#include <vector>
#include <functional>
#include <memory>

#include "imgui/imgui.h"

#include "fmt/core.h"
#include "fmt/ranges.h"

//Custom includes
#include "CoachX.h"

extern std::shared_ptr<CVarManagerWrapper> cvar_manager;
extern std::shared_ptr<GameWrapper> game_wrapper;
//TODO: extern std::shared_ptr<CoachX> _globalCoachXWrapper;
extern CoachX* coach_x_wrapper;

template<typename S, typename... Args>
void LOG(const S& format_str, Args&&... args)
{
	cvar_manager->log(fmt::format(format_str, args...));
}