// OptiLogger.cpp
#include "OptiLogger.h"
#include "OptiloggerStyle.h"
#include "OptiloggerCommands.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "OptiloggerWidget.h"

DEFINE_LOG_CATEGORY(LogOptiLogger);

static const FName OptiLoggerTabName("OptiLogger");

#define LOCTEXT_NAMESPACE "FOptiLogger"

void FOptiLogger::StartupModule()
{
    // Inicializar estilo y comandos
    FOptiloggerStyle::Initialize();
    FOptiloggerStyle::ReloadTextures();
    FOptiloggerCommands::Register();

    // Crear lista de comandos y mapear acción
    PluginCommands = MakeShareable(new FUICommandList);
    PluginCommands->MapAction(
        FOptiloggerCommands::Get().OpenPluginWindow,
        FExecuteAction::CreateRaw(this, &FOptiLogger::PluginButtonClicked),
        FCanExecuteAction()
    );

    // Registrar callback para menús/toolbars
    UToolMenus::RegisterStartupCallback(
        FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FOptiLogger::RegisterMenus)
    );

    // Registrar la pestaña Nomad (oculta por defecto)
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        OptiLoggerTabName,
        FOnSpawnTab::CreateRaw(this, &FOptiLogger::OnSpawnPluginTab)
    )
    .SetDisplayName(LOCTEXT("OptiLoggerTabTitle", "OptiLogger"))
    .SetMenuType(ETabSpawnerMenuType::Hidden);

    UE_LOG(LogOptiLogger, Log, TEXT("OptiLogger started."));
}

void FOptiLogger::ShutdownModule()
{
    // Limpiar menús y pestañas
    UToolMenus::UnRegisterStartupCallback(this);
    UToolMenus::UnregisterOwner(this);
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(OptiLoggerTabName);

    // Desregistrar comandos y estilo
    FOptiloggerCommands::Unregister();
    FOptiloggerStyle::Shutdown();

    UE_LOG(LogOptiLogger, Log, TEXT("OptiLogger shut down."));
}

void FOptiLogger::PluginButtonClicked()
{
    FGlobalTabmanager::Get()->TryInvokeTab(OptiLoggerTabName);
}

TSharedRef<SDockTab> FOptiLogger::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
    [
        SNew(SOptiloggerWidget)
    ];
}

void FOptiLogger::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);

    // Menú Window → Open Plugin Window
    if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window"))
    {
        FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
        Section.AddMenuEntryWithCommandList(FOptiloggerCommands::Get().OpenPluginWindow, PluginCommands);
    }

    // Toolbar del Level Editor
    if (UToolMenu* Toolbar = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar"))
    {
        FToolMenuSection& Section = Toolbar->FindOrAddSection("Settings");
        FToolMenuEntry& Entry = Section.AddEntry(
            FToolMenuEntry::InitToolBarButton(FOptiloggerCommands::Get().OpenPluginWindow)
        );
        Entry.SetCommandList(PluginCommands);
    }
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FOptiLogger, OptiLogger)
