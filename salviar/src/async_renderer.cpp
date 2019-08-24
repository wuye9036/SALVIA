#include <salviar/include/async_renderer.h>

#include <salviar/include/renderer_impl.h>
#include <salviar/include/render_core.h>
#include <eflib/include/memory/bounded_buffer.h>
#include <eflib/include/diagnostics/assert.h>

#include <memory>
#include <mutex>
#include <atomic>

using std::vector;

BEGIN_NS_SALVIAR()

EFLIB_DECLARE_CLASS_SHARED_PTR(renderer);

static const uint32_t ASYNC_RENDER_QUEUE_SIZE = 32;

class async_renderer: public renderer_impl
{
public:
    async_renderer()
        : state_queue_(ASYNC_RENDER_QUEUE_SIZE)
        , state_pool_(ASYNC_RENDER_QUEUE_SIZE)
        , waiting_exit_(false)
    {
        for(auto& state: state_pool_)
        {
            state.reset(new render_state());
        }
    }

	~async_renderer()
	{
		release();
	}

	virtual result flush()
	{
        while(object_count_in_pool() != MAX_COMMAND_QUEUE)
        {
            std::this_thread::yield();
        }
		return result::ok;
	}

	void run()
	{
		rendering_thread_ = std::thread(&async_renderer::do_rendering, this);
	}

private:
	render_state_ptr alloc_render_state()
    {
        for(;;)
        {
            {
                std::lock_guard<std::mutex> pool_lock(state_pool_mutex_);

                if(!state_pool_.empty())
                {
                    auto ret = state_pool_.back();
                    state_pool_.pop_back();
                    return ret;
                }
            }

            std::this_thread::yield();
        }
    }

    void free_render_state(render_state_ptr const& state)
    {
        std::lock_guard<std::mutex> pool_lock(state_pool_mutex_);
        state_pool_.push_back(state);
    }

    size_t object_count_in_pool() const
    {
        std::lock_guard<std::mutex> pool_lock(state_pool_mutex_);
        return state_pool_.size();
    }

    virtual result commit_state_and_command()
    {
        auto dest_state = alloc_render_state();
        copy_using_state(dest_state.get(), state_.get());
        state_queue_.push_front(dest_state);

        return result::ok;
    }

	result release()
	{
		if ( rendering_thread_.joinable() )
		{
            waiting_exit_ = true;
            state_queue_.push_front(render_state_ptr());
			rendering_thread_.join();
		}
		return result::ok;
	}

	void do_rendering()
	{
        while(!waiting_exit_)
		{
            render_state_ptr rendering_state;
            state_queue_.pop_back(&rendering_state);

            if(rendering_state)
            {
                core_.update(rendering_state);
                core_.execute();
                free_render_state(rendering_state);
            }
		}
	}

    mutable std::mutex                              state_pool_mutex_;
    mutable eflib::bounded_buffer<render_state_ptr> state_queue_;
	mutable std::vector<render_state_ptr>           state_pool_;

	std::thread	                                    rendering_thread_;
    std::atomic<bool>                               waiting_exit_;                                                          
};

renderer_ptr create_async_renderer()
{
    auto ret = std::make_shared<async_renderer>();
	ret->run();
	return ret;
}

END_NS_SALVIAR()
