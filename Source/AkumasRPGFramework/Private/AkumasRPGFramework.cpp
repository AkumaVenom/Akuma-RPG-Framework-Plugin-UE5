#include "AkumasRPGFramework.h"
#include "Modules/ModuleManager.h"
DEFINE_LOG_CATEGORY(LogARPG);
class FAkumasRPGFrameworkModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override { UE_LOG(LogARPG, Log, TEXT("Akuma's RPG Framework loaded.")); }
    virtual void ShutdownModule() override {}
};
IMPLEMENT_MODULE(FAkumasRPGFrameworkModule, AkumasRPGFramework)
