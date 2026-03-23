#include "app/EntryPoint.h"
#include "modules/Module.h"
#include "modules/ModuleRegistry.h"

class WaystoneModule : public Wayfinder::Module
{
    void Register(Wayfinder::ModuleRegistry& /*registry*/) override {}
};

std::unique_ptr<Wayfinder::Module> Wayfinder::CreateModule()
{
    return std::make_unique<WaystoneModule>();
}