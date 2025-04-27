#pragma once
#include "core/Application.h"
#include "core/EntryPoint.h"

namespace Waystone
{
    class WaystoneApplication : public Wayfinder::Application
    {
    public:
        WaystoneApplication() : Application(Config{
                                    .screenWidth = 1280,
                                    .screenHeight = 720,
                                    .windowTitle = "Waystone Sandbox"}) {}
    };
}

Wayfinder::Application* Wayfinder::CreateApplication(const Wayfinder::Application::CommandLineArgs& args)
{
    return new Waystone::WaystoneApplication();
}