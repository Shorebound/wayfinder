#pragma once
#include "Application.h"

#include <memory>

int main(int argc, char* argv[])
{
    auto app = Wayfinder::CreateApplication({argc, argv});
    if (app)
    {
        app->Run();
    }
    return 0;
}
