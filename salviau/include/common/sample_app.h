#pragma once

#include <salviau/include/salviau_forward.h>

#include <salviau/include/common/timer.h>

#include <salviax/include/resource/texture/tex_io.h>
#include <salviax/include/swap_chain/swap_chain.h>

#include <salviar/include/async_object.h>
#include <salviar/include/surface.h>
#include <salviar/include/renderer.h>
#include <eflib/include/diagnostics/profiler.h>

#include <string>
#include <vector>
#include <memory>

namespace salviar
{
	EFLIB_DECLARE_CLASS_SHARED_PTR(surface);
}

BEGIN_NS_SALVIAU()

class gui;

enum class app_modes
{
	unknown,
	benchmark,		// Run as benchmark. It will generate some benchmark results.
	test,			// Run as regression test. It will generate final frames as image file for test.
	interactive,	// Interactive mode.
	replay,			// Play mode.
	count
};

enum class quit_conditions
{
	user_defined	= 0,
	frame_limits	= 1,
	time_out		= 2,
};

struct frame_data
{
	salviar::pipeline_statistics pipeline_stat;
	salviar::internal_statistics internal_stat;
	salviar::pipeline_profiles   pipeline_prof;
};

struct sample_app_data
{
public:
	std::string					benchmark_name;
	
	app_modes					mode;
	uint32_t					screen_width;
	uint32_t					screen_height;
	float						screen_aspect_ratio;
	salviar::viewport			screen_vp;

	std::optional<bool>		    is_sync_renderer;
	salviax::swap_chain_types	sc_type;

	salviau::gui*				gui;
	salviar::renderer_ptr		renderer;
	salviax::swap_chain_ptr		swap_chain;
	salviar::surface_ptr		color_target;
	salviar::surface_ptr		resolved_color_target;
	salviar::surface_ptr		ds_target;

	double						total_elapsed_sec;
	double						elapsed_sec;
	size_t						frame_count;

	eflib::profiler				prof;
	
	salviar::async_object_ptr   pipeline_stat_obj;
    salviar::async_object_ptr   internal_stat_obj;
	salviar::async_object_ptr	pipeline_prof_obj;

	std::vector<frame_data>		frame_profs;
	quit_conditions				quit_cond;
	uint32_t					quit_cond_data; // frame count or millisecond.
	bool						runnable;
	bool						quiting;
	timer						frame_timer;
	timer						second_timer;
	int							frames_in_second;
};

class SALVIAU_API sample_app
{
public:
	sample_app(std::string const& app_name);
	virtual ~sample_app();

	// Called by client
	void run();
	void init(int argc, std::_tchar const** argv);

protected:
	virtual void profiling				(std::string const& stage_name, std::function<void()> const& fn);

	void create_devices_and_targets(
		size_t width, size_t height,
		size_t sample_count,
		salviar::pixel_format color_fmt, salviar::pixel_format ds_format
	);
	
	void quit_at_frame(uint32_t frame_cnt);
	void quit_if_time_out(uint32_t milli_sec);

	void quit();

	// Events
	virtual void on_init() = 0;
	virtual void on_frame() = 0;

protected:
	std::unique_ptr<sample_app_data> data_;

private:
	void init_params(int argc, std::_tchar const** argv);
	void draw_frame();
	
	virtual void save_frame				(salviar::surface_ptr const& surf);
	virtual void save_profiling_result	();

	void on_gui_idle();
	void on_gui_draw();
};

END_NS_SALVIAU()
