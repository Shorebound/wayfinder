#pragma once
#include "Application.h"
#include "plugins/Plugin.h"

#include <memory>

int main(int argc, char* argv[])
{
    auto gamePlugin = Wayfinder::Plugins::CreateGamePlugin();
    Wayfinder::Application app(std::move(gamePlugin), {argc, argv});
    app.Run();
    return 0;
}
