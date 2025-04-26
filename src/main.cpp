#include "core/Application.h"

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
    // Forward declaration for web platform
    void UpdateDrawFrame(void);
#endif

int main()
{
    // Create and initialize the application
    Wayfinder::Application app;
    app.Run();
}
