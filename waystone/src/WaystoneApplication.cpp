#pragma once
#include "core/Application.h"
#include "core/EntryPoint.h"

namespace Waystone
{
    class WaystoneApplication : public Wayfinder::Application
    {
    public:
        WaystoneApplication(const Config& config = {}) : Application(config) {}
    };
}

Wayfinder::Application* Wayfinder::CreateApplication(const Wayfinder::Application::CommandLineArgs& args)
{
    auto config = Wayfinder::Application::Config{
        .ScreenWidth = 1920,
        .ScreenHeight = 1080,
        .WindowTitle = "Waystone Sandbox",
        .VSync = false};
    return new Waystone::WaystoneApplication(config);
}