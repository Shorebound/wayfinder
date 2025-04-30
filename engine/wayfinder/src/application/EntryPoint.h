#pragma once
#include "Application.h"


int main(int argc, char* argv[]) 
{
    auto* app = Wayfinder::CreateApplication({argc, argv});
    app->Run();
    delete app;
}
