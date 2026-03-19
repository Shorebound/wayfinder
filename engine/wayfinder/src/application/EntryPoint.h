#pragma once
#include "Application.h"
#include "../core/GameModule.h"

#include <memory>

int main(int argc, char* argv[])
{
    auto gameModule = Wayfinder::CreateGameModule();
    Wayfinder::Application app(std::move(gameModule), {argc, argv});
    app.Run();
    return 0;
}
