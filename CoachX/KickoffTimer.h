#pragma once
#include <array>

class CoachX;

class KickoffTimer
{
public:
	void enable();
	void disable();
	void on_join();
	void on_leave();
	[[nodiscard]] bool is_active() const;
	void handle_tick(ControllerInput* input);

	void render_canvas(CanvasWrapper* canvas);
	void render_settings();
	void update_settings();

private:
	bool enabled_ = false; // Feature is requested by the player
	bool active_ = false; // Feature is active_ in current game mode

	struct config {
		// Features
		bool enable_freeplay = true;
		bool enable_training = true;
		bool limit_boost_freeplay = true;
		float restore_unlimited_boost_delay = 0.8f;
		bool limit_boost_training = true;
		bool wait_for_landing_freeplay = true;
		bool wait_for_landing_training = true;

		//Rendering
		bool show_spawn = true;
		bool show_guidance = true;
		bool show_personal_best = true;
		bool show_time = true;

		Vector2F render_position = { 0.5f, 0.1f };
		float text_scale = 3.0f;
		float draw_time = 4.0f;
	};
	config cfg_{};

	[[nodiscard]] inline bool is_in_game() const;

	const std::array<const std::string, 6> spawn_locations_ = {
		// Do not change order!
		"None",
		"Left",
		"Right",
		"Middle",
		"Middle Left",
		"Middle Right"
	};
	const std::array<const float, 6> spawn_guidance_timings_ = {
		// Do not change order!
		-1.0f,
		2.069f,
		2.069f,
		2.629f,
		2.303f,
		2.303f
	};
	std::array<float, 6> personal_best_timings_ = {
		// Do not change order!
		-1.0f,
		std::numeric_limits<float>::infinity(),
		std::numeric_limits<float>::infinity(),
		std::numeric_limits<float>::infinity(),
		std::numeric_limits<float>::infinity(),
		std::numeric_limits<float>::infinity()
	};
	std::filesystem::path save_file_;
	std::filesystem::path config_file_;
	void save_personal_best_timings() const;
	void load_personal_best_timings();
	void save_config() const;
	void load_config();

	static unsigned int get_spawn_location();
	unsigned int last_spawn_ = 0;

	const ControllerInput no_input_ = { };
	const float landing_delay_ = 0.3f;
	bool wait_for_landing_ = false;
	float respawn_time_ = -1.0f;

	bool kickoff_custom_training_ = false;
	bool respawned_ = false;
	float started_driving_ = -1.0f;

	std::unique_ptr<CVarWrapper> cl_soccar_boostcounter_;
	bool cl_soccar_boostcounter_value_ = false;
	bool automatic_limit_boost_ = false;
	bool limited_ = false;
	bool unlimited_boost_ = false;
	float ball_hit_ = -1.0f;

	void on_kickoff_custom_training(ActorWrapper& actor_wrapper);
	void on_respawn();
	void on_ball_hit();

	bool valid_draw_values_ = false;
	float last_text_update_ = -1;

	static constexpr int amount_text_lines = 4;
	std::array<std::string, amount_text_lines> text_lines_ = {"Kickoff:    None", "Guidance: 0.000", "PB:          0.000", "Time:       0.000"};
	std::array<Vector2, amount_text_lines> text_locations_ = std::array<Vector2, amount_text_lines>();
	//TODO: Move to config and save it
	std::array<LinearColor, 4> text_colors_ = {
		LinearColor{ 255, 255, 255, 255 }, // White
		LinearColor{ 0, 255, 0, 255 }, // Green
		LinearColor{ 255, 0, 0, 255 }, // Red
		LinearColor{ 255, 165, 0, 255 } // Yellow
	};
	std::array<LinearColor, 4> text_colors_imgui_ = {
		LinearColor{ text_colors_[0].R / 255, text_colors_[0].G / 255,text_colors_[0].B / 255,text_colors_[0].A / 255 },
		LinearColor{ text_colors_[1].R / 255, text_colors_[1].G / 255,text_colors_[1].B / 255,text_colors_[1].A / 255 },
		LinearColor{ text_colors_[2].R / 255, text_colors_[2].G / 255,text_colors_[2].B / 255,text_colors_[2].A / 255 },
		LinearColor{ text_colors_[3].R / 255, text_colors_[3].G / 255,text_colors_[3].B / 255,text_colors_[3].A / 255 }
	};
	std::array<int, amount_text_lines> text_line_colors_ = { 0, 1, 2, 3 };

	static bool is_equal(const float a, const float b, const float epsilon = 0.005f) {
		return (fabs(a - b) < epsilon);
	}
};

