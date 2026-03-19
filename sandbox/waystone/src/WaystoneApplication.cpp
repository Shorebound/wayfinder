#include "core/Module.h"
#include "core/ModuleRegistry.h"
#include "application/EntryPoint.h"

class WaystoneModule : public Wayfinder::Module
{
    void Register(Wayfinder::ModuleRegistry& /*registry*/) override {}
};

std::unique_ptr<Wayfinder::Module> Wayfinder::CreateModule()
{
    return std::make_unique<WaystoneModule>();
}