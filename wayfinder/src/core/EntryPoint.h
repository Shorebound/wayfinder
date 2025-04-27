#pragma once
#include "Application.h"

extern Wayfinder::Application* Wayfinder::CreateApplication(const Application::CommandLineArgs& args);

#if !defined(WAYFINDER_CUSTOM_MAIN)
int main(int argc, char** argv)
{
    auto* app = Wayfinder::CreateApplication({argc, argv});
    app->Run();
    delete app;
}
#endif

