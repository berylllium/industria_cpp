#include "client/platform/platform.hpp"
#include "client/renderer/renderer.hpp"
#include "client/clock.hpp"
#include "client/event.hpp"
#include "client/input.hpp"

#include <simple-logger.hpp>

void on_window_close(uint16_t event_code, EventContext ctx);

static struct
{
    bool is_running = true;

    Clock delta_clock;
    double delta_time;
} client_state;

int main()
{
    sl::log_info("Initializing...");

    // Initialize.
    event_init();
    platform_init("Industria", 100, 100, 400, 400);
    renderer_initialize();

    event_add_listener(EventCodes::ON_WINDOW_CLOSE, on_window_close);

    client_state.delta_clock.reset();

    sl::log_info("Starting game loop.");

    while (client_state.is_running)
    {
        // Calculate delta time.
		client_state.delta_time = client_state.delta_clock.get_elapsed_time();
		client_state.delta_clock.reset();

        // Poll platform messages.
		if (!platform_poll_messages())
		{
			sl::log_fatal("Failed to poll platform messages");
			client_state.is_running = false;
		}

        // Begin frame.
        if (!renderer_begin_frame())
        {
            sl::log_fatal("Failed to begin rendering a new frame.");

            return -1;
        }

        // End frame.
        if (!renderer_end_frame())
        {
            sl::log_fatal("Failed to end rendering a new frame.");

            return -1;
        }

        input_update();
    }
    
    renderer_shutdown();
    platform_shutdown();
    event_shutdown();

    return 0;
}

void on_window_close(uint16_t event_code, EventContext ctx)
{
	client_state.is_running = false;
}
