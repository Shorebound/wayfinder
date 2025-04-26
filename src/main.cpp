#include "../include/Application.h"

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
    // Forward declaration for web platform
    void UpdateDrawFrame(void);
#endif

int main()
{
    // Create and initialize the application
    Wayfinder::Application app;
    app.Initialize(800, 450, "Wayfinder Engine");

#if defined(PLATFORM_WEB)
    // For web platform, we need to use emscripten's main loop
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    // Run the application (contains the main game loop)
    app.Run();
#endif

    return 0;
}

#if defined(PLATFORM_WEB)
// For web platform compatibility
void UpdateDrawFrame(void)
{
    // This would need to be implemented for web platform
    // We would need to make the Application instance accessible here
}
#endif