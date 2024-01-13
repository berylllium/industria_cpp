#include "client/platform/platform.hpp"
#include "client/renderer/renderer.hpp"

int main()
{
    platform_init("Industria", 100, 100, 400, 400);
    renderer_initialize();
    
    renderer_shutdown();
    platform_shutdown();

    return 0;
}
