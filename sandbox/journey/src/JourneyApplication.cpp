#include "application/Application.h"
#include "application/EntryPoint.h"

std::unique_ptr<Wayfinder::Application> Wayfinder::CreateApplication(
    const Wayfinder::Application::CommandLineArgs& args)
{
    return std::make_unique<Application>("engine.toml", args);
}