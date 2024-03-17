#include "handler/voxel_handler.hpp"
#include "platform/platform.hpp"
#include "renderer/renderer.hpp"
#include "voxel/voxel_grid.hpp"
#include "clock.hpp"
#include "event.hpp"
#include "input.hpp"

#include <simple-logger.hpp>

void on_window_close(uint16_t event_code, EventContext ctx);

static struct
{
    bool is_running = true;

    Clock delta_clock;
    double delta_time;

    VoxelGrid test_grid;
} client_state;

bool client_initialize();
bool client_run();
void client_shutdown();

int main()
{
    if (!client_initialize())
    {
        sl::log_fatal("Failed to initialize the client.");
        return -1;
    }

    if (!client_run())
    {
        sl::log_fatal("Client didn't gracefully shut down.");
        return -1;
    }

    client_shutdown();

    return 0;
}

bool client_initialize()
{
    sl::log_info("Initializing...");

    // Initialize.
    if (!event_init())
    {
        sl::log_fatal("Failed to initialize the event subsystem.");
        return false;
    }

    if (!platform_init("Industria", 100, 100, 400, 400))
    {
        sl::log_fatal("Failed to initialize the platform subsystem.");
        return false;
    }

    if (!renderer_initialize())
    {
        sl::log_fatal("Failed to initialize the renderer subsystem.");
        return false;
    }

    event_add_listener(EventCodes::ON_WINDOW_CLOSE, on_window_close);

    voxel_handler_register_voxel("sand", Voxel { vector4f { 1.0f, 0.98f, 0.725f, 1.0f } } );
    voxel_handler_register_voxel("grass", Voxel { vector4f { 0.459f, 0.741f, 0.392f, 1.0f } } );

    client_state.test_grid = std::move(VoxelGrid::create(4, 0.1f).value());

    client_state.test_grid.set_voxel({0, 0, 0}, *voxel_handler_get_voxel_index("sand"));
    client_state.test_grid.set_voxel({1, 0, 0}, *voxel_handler_get_voxel_index("grass"));

    client_state.delta_clock.reset();

    return true;
}

bool client_run()
{
    sl::log_info("Starting game loop.");

    bool error_happened = false;

    static int frames = 0;
    static float acc = 0;

    while (client_state.is_running)
    {
        // Calculate delta time.
		client_state.delta_time = client_state.delta_clock.get_elapsed_time();
		client_state.delta_clock.reset();

        acc += client_state.delta_time;
        frames++;

        if (acc >= 1.0f)
        {
            sl::log_debug("It has been {} seconds with an average frame time of {} seconds and frame rate of {} fps.", acc, acc / frames, frames/acc);

            frames = 0;
            acc = 0;
        }

        // Poll platform messages.
		if (!platform_poll_messages())
		{
			sl::log_fatal("Failed to poll platform messages");

			client_state.is_running = false;
            error_happened = true;
		}

        // Begin frame.
        if (!renderer_begin_frame())
        {
            sl::log_fatal("Failed to begin rendering a new frame.");

			client_state.is_running = false;
            error_happened = true;
        }

        // End frame.
        if (!renderer_end_frame())
        {
            sl::log_fatal("Failed to end rendering a new frame.");

			client_state.is_running = false;
            error_happened = true;
        }

        input_update();
    }

    return !error_happened;
}

void client_shutdown()
{
    renderer_shutdown();
    platform_shutdown();
    event_shutdown();

    sl::log_info("Successfully shut down all systems.");
}

void on_window_close(uint16_t event_code, EventContext ctx)
{
	client_state.is_running = false;
}
