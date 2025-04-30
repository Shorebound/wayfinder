#pragma once
#include "application/Application.h"
#include "application/EntryPoint.h"

namespace Journey
{
    class JourneyApplication : public Wayfinder::Application
    {
    public:
        JourneyApplication(const Config& config = {}) : Application(config) {}
    };
}

Wayfinder::Application* Wayfinder::CreateApplication(const Wayfinder::Application::CommandLineArgs& args)
{
    const auto config = Application::Config{
        .ScreenWidth = 1920,
        .ScreenHeight = 1080,
        .WindowTitle = "Journey Sandbox",
        .VSync = false};
    return new Journey::JourneyApplication(config);
}