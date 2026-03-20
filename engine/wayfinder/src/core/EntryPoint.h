#pragma once
#include "Application.h"
#include "Module.h"

#include <memory>

int main(int argc, char* argv[])
{
    auto module = Wayfinder::CreateModule();
    Wayfinder::Application app(std::move(module), {argc, argv});
    app.Run();
    return 0;
}
