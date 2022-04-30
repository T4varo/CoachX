#include "pch.h"
#include "KickoffTimer.h"
#include <fstream>

void KickoffTimer::enable() {
	if (!enabled_) {
		save_file_ = game_wrapper->GetBakkesModPath().append("data\\coachx_kickofftimer_pb.bin");
		config_file_ = game_wrapper->GetBakkesModPath().append("data\\coachx_kickofftimer_config.bin");
		load_personal_best_timings();
		load_config();

		/* Join: Function TAGame.FXActor_Car_TA.PostBeginPlay
		* Function TAGame.Team_TA.PostBeginPlay fires to early in custom training
		* Function TAGame.FXActor_Car_TA.PostBeginPlay used for reset
		* Maybe:
		* Function TAGame.GameEvent_Soccar_TA.InitGame
		* Function TAGame.GameEvent_Soccar_TA.InitField
		* Function TAGame.GameEvent_Soccar_TA.OnAllTeamsCreated
		* Function TAGame.GameEvent_Soccar_TA.OnBallSpawned
		* Function TAGame.GameEvent_Soccar_TA.OnInit
		*/
		coach_x_wrapper->HookEventPost("Function TAGame.FXActor_Car_TA.PostBeginPlay", [this](const std::string& event_name) {
			if (enabled_ && !active_ && is_in_game()) on_join();
		});

		/* Leave: Function TAGame.GameEvent_Soccar_TA.Destroyed
		* Gets fired in replays when seeking
		* Maybe try to check if isInReplay is still true
		*/
		coach_x_wrapper->HookEventPost("Function TAGame.GameEvent_Soccar_TA.Destroyed", [this](const std::string& event_name) {
			if (active_) on_leave();
		});

		// check if in KickoffCustomTraining
		coach_x_wrapper->HookEventWithCallerPost<ActorWrapper>("Function TAGame.GameEvent_TrainingEditor_TA.LoadRound", [this](ActorWrapper actor_wrapper, void* params, const std::string& event_name) {
			on_kickoff_custom_training(actor_wrapper);
		});

		//	update drawing values when screen resolution changes
		coach_x_wrapper->HookEventPost("Function TAGame.GFxData_Settings_TA.GetResolution", [this](const std::string& event_name) {
			valid_draw_values_ = false;
		});

		cl_soccar_boostcounter_ = std::make_unique<CVarWrapper>(cvar_manager->getCvar("cl_soccar_boostcounter"));

		cvar_manager->registerNotifier("coachx_kickofftimer_training", [this](...) {
			cvar_manager->executeCommand("load_training 2426-9BBA-9BD7-53FB");
		}, "", PERMISSION_ALL);

		enabled_ = true;
		LOG("CoachX KickoffTimer enabled!");
		if (is_in_game()) on_join();

		// TODO: Remove this:
		/*
		cvar_manager->registerNotifier("coachx_debug", [this](...) {

		}, "", PERMISSION_ALL);
		*/
	}
}

void KickoffTimer::disable() {
	save_personal_best_timings();
	save_config();
	on_leave();
	coach_x_wrapper->UnhookEventPost("Function TAGame.FXActor_Car_TA.PostBeginPlay");
	coach_x_wrapper->UnhookEventPost("Function TAGame.GameEvent_Soccar_TA.Destroyed");
	coach_x_wrapper->UnhookEventPost("Function TAGame.GameEvent_TrainingEditor_TA.LoadRound");
	coach_x_wrapper->UnhookEventPost("Function TAGame.GFxData_Settings_TA.GetResolution");
	enabled_ = false;
}

void KickoffTimer::on_join() {
	try {
		//// Event Hooks
		// Reset
		// Function TAGame.CarComponent_Boost_TA.GiveStartingBoost
		// Function TAGame.CarComponent_Boost_TA.OnCreated
		// Function TAGame.FXActor_TA.PostBeginPlay (multiple times)
		// Function Engine.Actor.PostBeginPlay (multiple times)
		// Function TAGame.FXActor_Car_TA.PostBeginPlay
		coach_x_wrapper->HookEventPost("Function TAGame.CarComponent_Boost_TA.GiveStartingBoost", [this](const std::string& event_name) {
			if(is_in_game()) on_respawn();
		});
		// Ball Hit
		coach_x_wrapper->HookEventPost("Function TAGame.GameEvent_Soccar_TA.EventFirstBallHit", [this](const std::string& event_name) {
			if (is_in_game()) on_ball_hit();
		});
		// Start driving
		// Custom Training:
		// Function GameEvent_Soccar_TA.Countdown.EndState
		// TAGame.TrainingEditorMetrics_TA.TrainingShotAttempt
		// Freeplay:
		// Function TAGame.Car_TA.SetVehicleInput
		/*
		coach_x_wrapper->HookEventWithCaller<CarWrapper>("Function TAGame.Car_TA.SetVehicleInput", [this](const CarWrapper& car, void* params, const std::string& event_name) {
			if (is_in_game()) handle_tick(static_cast<ControllerInput*>(params));
		});
		*/

		update_settings();

		// Automatic limit boost
		coach_x_wrapper->HookEventPost("Function TAGame.CarComponent_Boost_TA.SetUnlimitedBoost", [this](const std::string& event_name) {
			if (automatic_limit_boost_ && respawned_) {
				if (game_wrapper->GetLocalCar().GetBoostComponent().GetUnlimitedBoostRefCount()) {
					cvar_manager->executeCommand("boost set 33");
					limited_ = true;
				} else if (!cl_soccar_boostcounter_->IsNull() && cl_soccar_boostcounter_->getBoolValue()) {
					cl_soccar_boostcounter_value_ = true;
					cl_soccar_boostcounter_->setValue(false);
					cvar_manager->executeCommand("boost set 33");
					limited_ = true;
				}
			}
		});
		

		/* Notes:
		* Activate Boost: Function TAGame.PlayerController_TA.ToggleBoost
		* Boost Consumed: Function TAGame.CarComponent_Boost_TA.EventBoostAmountChanged
		* Check if car landed before it got accelerated: Function TAGame.FXActor_Car_TA.SetBraking
		*/

		active_ = true;
		LOG("CoachX KickoffTimer activated!");
	} catch (const MultipleHookException &e) {
		LOG("Could not activate CoachX KickoffTimer:\n{}", e.what());
		on_leave();
	}
}

void KickoffTimer::update_settings() {
	automatic_limit_boost_ = (game_wrapper->IsInFreeplay() && cfg_.limit_boost_freeplay) || (kickoff_custom_training_ && cfg_.limit_boost_training);
	if (!is_in_game() && active_) on_leave();
}

void KickoffTimer::on_leave() {
	//// Event Unhooks
	// Reset
	coach_x_wrapper->UnhookEventPost("Function TAGame.CarComponent_Boost_TA.GiveStartingBoost");
	// Ball Hit
	coach_x_wrapper->UnhookEventPost("Function TAGame.GameEvent_Soccar_TA.EventFirstBallHit");
	// Start driving
	//coach_x_wrapper->UnhookEvent("Function TAGame.Car_TA.SetVehicleInput");
	// Automatic limit boost
	coach_x_wrapper->UnhookEventPost("Function TAGame.CarComponent_Boost_TA.SetUnlimitedBoost");

	if (cl_soccar_boostcounter_value_ && !cl_soccar_boostcounter_->IsNull()) {
		cl_soccar_boostcounter_->setValue(true);
	}

	// Reset to init values
	unlimited_boost_ = false;
	automatic_limit_boost_ = false;
	cl_soccar_boostcounter_value_ = false;
	limited_ = false;
	kickoff_custom_training_ = false;
	valid_draw_values_ = false;
	wait_for_landing_ = false;
	last_spawn_ = 0;
	respawn_time_ = -1.0f;
	ball_hit_ = -1.0f;
	last_text_update_ = -1.0f;

	active_ = false;
	LOG("CoachX KickoffTimer deactivated!");
}

bool KickoffTimer::is_active() const {
	return active_;
}

void KickoffTimer::on_respawn() {
	last_spawn_ = get_spawn_location();
	//LOG("Spawn: {}", last_spawn_); // DEBUG
	started_driving_ = -1.0f;
	if (last_spawn_ > 0) {
		respawned_ = true;
		if ((game_wrapper->IsInFreeplay() && cfg_.wait_for_landing_freeplay) || (kickoff_custom_training_ && cfg_.wait_for_landing_training)) wait_for_landing_ = true;
		respawn_time_ = game_wrapper->GetGameEventAsServer().GetSecondsElapsed();
	}
}

void KickoffTimer::handle_tick(ControllerInput* input) {
	if (game_wrapper->GetGameEventAsServer().IsNull()) return;
	// Check if car started driving
	if (respawned_) {
		if (!game_wrapper->GetLocalCar().IsNull() && !game_wrapper->GetLocalCar().GetBoostComponent().IsNull()) {
			if (game_wrapper->GetLocalCar().GetForwardSpeed() > 3.0f) { // If car started driving
				//LOG("Car speed: {}", game_wrapper->GetLocalCar().GetForwardSpeed());
				unlimited_boost_ = game_wrapper->GetLocalCar().GetBoostComponent().GetUnlimitedBoostRefCount() > 0;
				respawned_ = false;
				started_driving_ = game_wrapper->GetGameEventAsServer().GetSecondsElapsed();
			}
		}
	}

	// Check if ball is still in the center of the field
	if (respawned_ || started_driving_ > -1.0f) {
		if (!game_wrapper->GetGameEventAsServer().GetBall().IsNull()) {
			const Vector ball_location = game_wrapper->GetGameEventAsServer().GetBall().GetLocation();
			if (!is_equal(ball_location.X, 0.0f) || !is_equal(ball_location.Y, 0.0f)) {
				respawned_ = false;
				started_driving_ = -1;
				// Enable unlimited boost if automatic limiting is active or in kickoff custom training
				if (automatic_limit_boost_) {
					if (cl_soccar_boostcounter_value_) {
						cl_soccar_boostcounter_->setValue(true);
					} else {
						cvar_manager->executeCommand("boost set unlimited");
					}
				}
			}
		}
	}

	// Restore Unlimited boost after <unlimited_boost_delay_> seconds if automatic limiting is activated
	if (limited_ && ball_hit_ > 1.0f) {
		if (game_wrapper->GetGameEventAsServer().GetSecondsElapsed() - ball_hit_ > cfg_.restore_unlimited_boost_delay) {
			ball_hit_ = -1.0f;
			if (cl_soccar_boostcounter_value_ && !cl_soccar_boostcounter_->IsNull()) {
				cl_soccar_boostcounter_value_ = false;
				cl_soccar_boostcounter_->setValue(true);
			} else {
				cvar_manager->executeCommand("boost set unlimited");
			}
			limited_ = false;
		}
	}

	// Wait until car has fully landed after respawn
	if (wait_for_landing_) {
		if (game_wrapper->GetGameEventAsServer().GetSecondsElapsed() - respawn_time_ < landing_delay_) {
			*input = no_input_;
		} else {
			wait_for_landing_ = false;
		}
	}
}

void KickoffTimer::on_ball_hit() {
	if (game_wrapper->GetGameEventAsServer().IsNull()) return;
	if (started_driving_ > -1.0f && !unlimited_boost_) {
		ball_hit_ = game_wrapper->GetGameEventAsServer().GetSecondsElapsed();
		float kickoff_time = ball_hit_ - started_driving_;
		started_driving_ = -1.0f;

		unsigned int i = 0;

		if (cfg_.show_spawn) {
			text_lines_[i] = "Kickoff:    " + spawn_locations_[last_spawn_];
			text_line_colors_[i++] = 0;
		}
		if (cfg_.show_guidance) {
			text_lines_[i] = fmt::format("Guidance: {}", spawn_guidance_timings_[last_spawn_]);
			text_line_colors_[i++] = spawn_guidance_timings_[last_spawn_] > kickoff_time ? 1 : 2; // Guidance color
		}
		if (cfg_.show_personal_best) {
			text_lines_[i] = fmt::format("PB:          {:.3f}", personal_best_timings_[last_spawn_]);
			text_line_colors_[i++] = personal_best_timings_[last_spawn_] > kickoff_time ? 1 : 2; // PB color
		}
		if (cfg_.show_time) {
			text_lines_[i] = fmt::format("Time:       {:.3f}", kickoff_time);
			text_line_colors_[i++] = spawn_guidance_timings_[last_spawn_] > kickoff_time // Time color
				? (personal_best_timings_[last_spawn_] > kickoff_time ? 1 : 3) // Time < Guidance: Green if you beat your PB, yellow otherwise
				: (personal_best_timings_[last_spawn_] > kickoff_time ? 3 : 2); // Time > Guidance: Yellow if you beat your PB, red otherwise
		}
		for (; i < text_lines_.size(); i++) {
			text_lines_[i] = "";
		}

		if (personal_best_timings_[last_spawn_] > kickoff_time) {
			personal_best_timings_[last_spawn_] = kickoff_time;
		}

		last_text_update_ = ball_hit_;
		valid_draw_values_ = false;
	}
}

void KickoffTimer::on_kickoff_custom_training(ActorWrapper& actor_wrapper) {
	if (actor_wrapper.IsNull()) return;

	TrainingEditorWrapper training_editor_wrapper = TrainingEditorWrapper(actor_wrapper.memory_address);
	if (training_editor_wrapper.IsNull() || training_editor_wrapper.GetTrainingData().IsNull() || training_editor_wrapper.GetTrainingData().GetTrainingData().IsNull() || training_editor_wrapper.GetTrainingData().GetTrainingData().GetCode().IsNull()) return;

	kickoff_custom_training_ = training_editor_wrapper.GetTrainingData().GetTrainingData().GetCode().ToString() == "2426-9BBA-9BD7-53FB";
}

unsigned int KickoffTimer::get_spawn_location() {
	if (game_wrapper->GetLocalCar().IsNull() ||	game_wrapper->GetGameEventAsServer().IsNull() || game_wrapper->GetGameEventAsServer().GetBall().IsNull()) return 0; // None

	//Check if ball is spawn normally
	if (const Vector ball_location = game_wrapper->GetGameEventAsServer().GetBall().GetLocation(); ball_location.X != 0.0f || ball_location.Y != 0.0f) {
		return 0; // None
	}

	const Vector car_pos = game_wrapper->GetLocalCar().GetLocation();
	//Determine Spawn
	if (is_equal(car_pos.X, 256.0f) && is_equal(car_pos.Y, -3840.0f)) {
		//LOG("Spawn: MiddleLeft");
		return 4; // Middle Left
	} else if (is_equal(car_pos.X, -256.0f) && is_equal(car_pos.Y, -3840.0f)) {
		//LOG("Spawn: MiddleRight");
		return 5; // Middle Right
	} else if (is_equal(car_pos.X, 0.0f) && is_equal(car_pos.Y, -4608.0f)) {
		//LOG("Spawn: Middle");
		return 3; // Middle
	} else if (is_equal(car_pos.X, 2048.0f) && is_equal(car_pos.Y, -2560.0f)) {
		//LOG("Spawn: Left");
		return 1; // Left
	} else if (is_equal(car_pos.X, -2048.0f) && is_equal(car_pos.Y, -2560.0f)) {
		//LOG("Spawn: Right");
		return 2; // Right
	}
	//LOG("Spawn: None");
	return 0; // None
}

inline bool KickoffTimer::is_in_game() const {
	return (cfg_.enable_freeplay && game_wrapper->IsInFreeplay()) || (cfg_.enable_training && game_wrapper->IsInCustomTraining() && kickoff_custom_training_);
}

void KickoffTimer::save_personal_best_timings() const {
	std::ofstream file;
	try {
		LOG("Saving PBs into: {}", save_file_.string()); // TODO: Remove this
		file.open(save_file_);
		if (file.is_open()) {
			for (unsigned long long  i = 1; i < personal_best_timings_.size(); i++) {
				file.write(reinterpret_cast<const char*>(&personal_best_timings_[i]), sizeof(float));
			}
			LOG("Saved KickoffTimer PBs!"); // TODO: Remove this
		} else {
			LOG("Error saving KickoffTimer PBs: Could not open KickoffTimer PB file.");
		}
	} catch (std::ofstream::failure &e) {
		LOG("Error: Could not write PB Timings to file:\n{}", e.what());
	}
	if (file.is_open()) {
		file.close();
	}
}

void KickoffTimer::load_personal_best_timings() {
	std::ifstream file;
	try {
		file.open(save_file_);
		if (file.is_open()) {
			for (unsigned long long i = 1; i < personal_best_timings_.size(); i++) {
				file.read(reinterpret_cast<char*>(&personal_best_timings_[i]), sizeof(float));
			}
		} else {
			LOG("Error loading KickoffTimer PBs: Could not open KickoffTimer PB file.");
		}
	} catch (std::ofstream::failure& e) {
		LOG("Error: Could not read PB Timings from file:\n{}", e.what());
	}
	if (file.is_open()) {
		file.close();
	}
}

void KickoffTimer::save_config() const {
	std::ofstream file;
	try {
		LOG("Saving KickoffTimer config into: {}", save_file_.string()); // TODO: Remove this
		file.open(config_file_);
		if (file.is_open()) {
			file.write(reinterpret_cast<const char*>(&cfg_), sizeof(config));
			LOG("KickoffTimer Config saved!"); // TODO: Remove this
		} else {
			LOG("Error saving KickoffTimer config: Cannot write file {}", config_file_.string());
		}
	} catch (std::ofstream::failure& e) {
		LOG("Could not write struct:\n{}", e.what());
	}
	if (file.is_open()) {
		file.close();
	}
}

void KickoffTimer::load_config() {
	std::ifstream file;
	try {
		file.open(config_file_);
		if (file.is_open()) {
			file.read(reinterpret_cast<char*>(&cfg_), sizeof(config));
		}
	} catch (std::ifstream::failure& e) {
		LOG("Could not load struct:\n{}", e.what());
	}
	if (file.is_open()) {
		file.close();
	}
}

void KickoffTimer::render_canvas(CanvasWrapper* canvas) {
	if (!is_in_game() || game_wrapper->GetGameEventAsServer().IsNull()) return;

	// Calculate draw values (position etc.)
	if (!valid_draw_values_) {
		// Calc longest text-line to center text
		Vector2F text_size = { -1.0f, -1.0f };
		for (auto const& text : text_lines_) {
			auto const& current_text_size = canvas->GetStringSize(text, cfg_.text_scale, cfg_.text_scale);
			if (current_text_size.X > text_size.X) {
				text_size = current_text_size;
			}
		}

		const Vector2 screen_size = game_wrapper->GetScreenSize();
		const float start_location_x = static_cast<float>(screen_size.X) * cfg_.render_position.X - text_size.X / 2.0f;
		const float start_location_y = static_cast<float>(screen_size.Y) * cfg_.render_position.Y;

		text_locations_[0] = { static_cast<int>(start_location_x), static_cast<int>(start_location_y) };
		for (int i = 1; i < amount_text_lines; i++) {
			text_locations_[i] = { static_cast<int>(start_location_x), static_cast<int>(start_location_y + text_size.Y * static_cast<float>(i)) };
		}

		valid_draw_values_ = true;
	}

	if (game_wrapper->GetGameEventAsServer().GetSecondsElapsed() - last_text_update_ < cfg_.draw_time) {
		for (int i = 0; i < amount_text_lines; i++) {
			canvas->SetPosition(text_locations_[i]);
			canvas->SetColor(text_colors_[text_line_colors_[i]]);
			canvas->DrawString(text_lines_[i], cfg_.text_scale, cfg_.text_scale, true);
		}
	}
}

void KickoffTimer::render_settings() {
	if (ImGui::Button("Load Training")) {
		game_wrapper->Execute([this] (const GameWrapper* game_wrapper) {
			cvar_manager->executeCommand("coachx_kickofftimer_training");
		});
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset Personal Best")) {
		for (unsigned long long i = 1; i < personal_best_timings_.size(); i++) {
			personal_best_timings_[i] = std::numeric_limits<float>::infinity();
		}
	}
	ImGui::TextUnformatted(R"(Tip: You can bind "coachx_kickofftimer_training" to any key under "F2 -> Bindings")");
	if (ImGui::CollapsingHeader("Settings")) {
		if (ImGui::Checkbox("Enable in freeplay", &cfg_.enable_freeplay)) update_settings();
		if (cfg_.enable_freeplay) {
			ImGui::Indent();
			if (ImGui::Checkbox("Limit boost on kickoff", &cfg_.limit_boost_freeplay)) update_settings();
			if (cfg_.limit_boost_freeplay) {
				ImGui::Indent();
				ImGui::TextUnformatted("Restore delay:");
				ImGui::SameLine(150);
				ImGui::SliderFloat("seconds##restore_delay", &cfg_.restore_unlimited_boost_delay, 0.0f, 5.0f, "%.2f");
				ImGui::Unindent();
			}
			ImGui::Checkbox("Block input until car landed", &cfg_.wait_for_landing_freeplay);
			ImGui::Unindent();
		}
		if (ImGui::Checkbox("Enable in Kickoff Training (reload of training necessary)", &cfg_.enable_training)) update_settings();
		if (cfg_.enable_training) {
			ImGui::Indent();
			if (ImGui::Checkbox("Limit boost", &cfg_.limit_boost_training)) update_settings();
			ImGui::Checkbox("Block input until car landed##1", &cfg_.wait_for_landing_training);
			ImGui::Unindent();
		}

		auto show_ui = [this](GameWrapper* game_wrapper) {
			if (!game_wrapper->GetCurrentGameState().IsNull()) {
				last_text_update_ = game_wrapper->GetCurrentGameState().GetSecondsElapsed();
				valid_draw_values_ = false;
			}
		};
		auto update_text_lines = [this, show_ui]() {
			unsigned int i = 0;
			if (cfg_.show_spawn) {
				text_lines_[i] = "Kickoff:    None";
				text_line_colors_[i++] = 0;
			}
			if (cfg_.show_guidance) {
				text_lines_[i] = "Guidance: 0.000";
				text_line_colors_[i++] = 1;
			}
			if (cfg_.show_personal_best) {
				text_lines_[i] = "PB:          0.000";
				text_line_colors_[i++] = 2;
			}
			if (cfg_.show_time) {
				text_lines_[i] = "Time:       0.000";
				text_line_colors_[i++] = 3;
			}
			for (; i < text_lines_.size(); i++) {
				text_lines_[i] = "";
			}
			game_wrapper->Execute(show_ui);
		};

		ImGui::TextUnformatted("Rendering:");
		ImGui::Indent();

		ImGui::TextUnformatted("Show:");
		ImGui::SameLine(150); if (ImGui::Checkbox("Spawn", &cfg_.show_spawn)) update_text_lines();
		ImGui::SameLine(300); if (ImGui::Checkbox("Guidance", &cfg_.show_guidance)) update_text_lines();
		ImGui::SameLine(450); if (ImGui::Checkbox("Personal Best", &cfg_.show_personal_best)) update_text_lines();
		ImGui::SameLine(600); if (ImGui::Checkbox("Time", &cfg_.show_time)) update_text_lines();

		ImGui::TextUnformatted("Colors:");
		ImGui::SameLine(150);
		
		if (ImGui::ColorEdit4("Default", reinterpret_cast<float*>(&text_colors_imgui_[0]), ImGuiColorEditFlags_NoInputs)) {
			text_colors_[0].R = text_colors_imgui_[0].R * 255;
			text_colors_[0].G = text_colors_imgui_[0].G * 255;
			text_colors_[0].B = text_colors_imgui_[0].B * 255;
			text_colors_[0].A = text_colors_imgui_[0].A * 255;
			game_wrapper->Execute(show_ui);
		}
		ImGui::SameLine(300);
		if (ImGui::ColorEdit4("Good", reinterpret_cast<float*>(&text_colors_imgui_[1]), ImGuiColorEditFlags_NoInputs)) {
			text_colors_[1].R = text_colors_imgui_[1].R * 255;
			text_colors_[1].G = text_colors_imgui_[1].G * 255;
			text_colors_[1].B = text_colors_imgui_[1].B * 255;
			text_colors_[1].A = text_colors_imgui_[1].A * 255;
			game_wrapper->Execute(show_ui);
		}
		ImGui::SameLine(450);
		if (ImGui::ColorEdit4("Ok", reinterpret_cast<float*>(&text_colors_imgui_[3]), ImGuiColorEditFlags_NoInputs)) {
			text_colors_[3].R = text_colors_imgui_[3].R * 255;
			text_colors_[3].G = text_colors_imgui_[3].G * 255;
			text_colors_[3].B = text_colors_imgui_[3].B * 255;
			text_colors_[3].A = text_colors_imgui_[3].A * 255;
			game_wrapper->Execute(show_ui);
		}
		ImGui::SameLine(600);
		if (ImGui::ColorEdit4("Bad", reinterpret_cast<float*>(&text_colors_imgui_[2]), ImGuiColorEditFlags_NoInputs)) {
			text_colors_[2].R = text_colors_imgui_[2].R * 255;
			text_colors_[2].G = text_colors_imgui_[2].G * 255;
			text_colors_[2].B = text_colors_imgui_[2].B * 255;
			text_colors_[2].A = text_colors_imgui_[2].A * 255;
			game_wrapper->Execute(show_ui);
		}

		ImGui::TextUnformatted("Position:");
		ImGui::SameLine(150);
		if (ImGui::SliderFloat("X", &cfg_.render_position.X, 0.0f, 1.0f, "%.3f")) {
			game_wrapper->Execute(show_ui);
		}
		ImGui::NewLine();
		ImGui::SameLine(150);
		if (ImGui::SliderFloat("Y", &cfg_.render_position.Y, 0.0f, 1.0f, "%.3f")) {
			game_wrapper->Execute(show_ui);
		}
		ImGui::TextUnformatted("Text Scale:");
		ImGui::SameLine(150);
		if (ImGui::SliderFloat("##Scale", &cfg_.text_scale, 0.0f, 20.0f, "%.2f")) {
			game_wrapper->Execute(show_ui);
		}
		ImGui::TextUnformatted("Draw time:");
		ImGui::SameLine(150);
		if (ImGui::SliderFloat("seconds##draw_time", &cfg_.draw_time, 0.0f, 20.0f, "%.2f")) {
			game_wrapper->Execute(show_ui);
		}
		ImGui::Separator();
	}
}