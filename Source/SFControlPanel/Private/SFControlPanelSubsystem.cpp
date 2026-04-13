#include "SFControlPanelSubsystem.h"

#include "BlueprintAssetHelperLibrary.h"
#include "Runtime/Core/Public/Logging/LogCategory.h"
#include "EngineUtils.h"
#include "FGBlueprintSubsystem.h"
#include "FGBuildGunBuild.h"
#include "FGCustomizationRecipe.h"
#include "FGRecipeManager.h"
#include "FGResourceScanner.h"
#include "FGUnlockSubsystem.h"
#include "Async/Async.h"
#include "StructuredLog.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "ImageUtils.h"
#include "NativeHookManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

us_listen_socket_t* SocketListener;
bool SocketRunning = false;

DECLARE_LOG_CATEGORY_CLASS(LogDeckMod, Log, All);

ADeckModSubsystem* ADeckModSubsystem::Get(UWorld* WorldContext)
{
	for (TActorIterator<ADeckModSubsystem> It(WorldContext, StaticClass(), EActorIteratorFlags::AllActors); It; ++It) {
		ADeckModSubsystem* CurrentActor = *It;
		return CurrentActor;
	};

	return nullptr;
}

ADeckModSubsystem::ADeckModSubsystem() : AModSubsystem()
{

}

ADeckModSubsystem::~ADeckModSubsystem()
{
	// Destructor ensures server is stopped if the actor is destroyed unexpectedly
	StopWebSocketServer();
}

void ADeckModSubsystem::BeginPlay()
{
	Super::BeginPlay();

	GenerateRecipes();
	StartWebSocketServer();
	
	// Register the callback to ensure WebSocket is stopped on crash/exit
	FCoreDelegates::OnExit.AddUObject(this, &ADeckModSubsystem::StopWebSocketServer);
}

void ADeckModSubsystem::StartWebSocketPushDataLoop()
{
	/* TODO: Modify for Deck WS

	if (bHasRunningPushDataLoop) return;
	
	Async(EAsyncExecution::Thread, [this]()
	{
		bHasRunningPushDataLoop = true;
		UE_LOGFMT(LogTemp, Log, "Starting PushUpdatedData loop");
		while (SocketRunning && !bShouldStop)
		{
			const float PushCycle = UFRMConfigManager::FRM_GetConfigOrDefault<float>(TEXT("uWS.PushCycle"), 5.0f);

			PushUpdatedData();
			FPlatformProcess::Sleep(PushCycle);
		}
		UE_LOGFMT(LogTemp, Log, "Stopped PushUpdatedData loop");
		bHasRunningPushDataLoop = false;
	});
	*/
}

void ADeckModSubsystem::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Ensure the server is stopped during normal gameplay exit
	StopWebSocketServer();
	Super::EndPlay(EndPlayReason);
}

void ADeckModSubsystem::StopWebSocketServer()
{
	bShouldStop = true;

    // Signal the WebSocket server to stop
    if (WebServer.IsValid())
    {
        WebServer.Reset();
    }

    // Close WebSocket listener
    if (SocketListener)
    {
        //UE_LOGFMT(LogTemp, Log, "Stopping uWS listener");
        us_listen_socket_close(0, SocketListener);
        SocketListener = nullptr;

        // TODO: Update logging components
		UE_LOG(LogTemp, Log, TEXT("Closing all %d connections"), ConnectedClients.Num());
        for (const auto ConnectedClient : ConnectedClients)
        {
            ConnectedClient->close();
        }
        ConnectedClients.Empty();
    }

    // clear endpoint subscribers
    EndpointSubscribers.Empty();
}

void ADeckModSubsystem::GenerateRecipes()
{

	AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(this->GetWorld());
	TArray<TSubclassOf<UFGRecipe>> Recipes = RecipeManager->GetAllRecipes();

	UE_LOGFMT(LogDeckMod, Log, "Num of Recipes found: {Recipes}", Recipes.Num());

	Recipes.Sort();
	
	for (auto Recipe : Recipes)
	{		
		// Skip over any UFGRecipe that doesn't have any build tools or outputs
		if (UFGRecipe::GetProducedIn(Recipe).Num() < 1 || UFGRecipe::GetProducts(Recipe).Num() < 1) continue;
		if (UKismetSystemLibrary::GetClassDisplayName(UFGRecipe::GetProducedIn(Recipe)[0]) == "BP_BuildGun_C")
			Descriptors.FindOrAdd(UKismetSystemLibrary::GetClassDisplayName(UFGRecipe::GetProducts(Recipe)[0].ItemClass), Recipe);
	}
}

void ADeckModSubsystem::StartWebSocketServer(bool bSkipIfRunning) 
{
    //UE_LOGFMT(LogTemp, Log, "Initializing WebSocket Service");

    if (SocketRunning)
    {
	    if (bSkipIfRunning)
    	{
	    	// TODO: Update logging components
			UE_LOG(LogTemp, Log, TEXT("Websocket Thread is already running. Stop start process."));

	    	return;
    	}

        // TODO: Update logging components
		UE_LOG(LogTemp, Log, TEXT("Old Websocket Thread is still running, try again in 3 seconds..."));

        AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this]()
        {
            // Sleep for the specified delay
            FPlatformProcess::Sleep(3.0f);
        
            // Then switch back to the game thread to run the task
            AsyncTask(ENamedThreads::GameThread, [this]()
            {
                StartWebSocketServer();
            });
        });
        return;
    }

        // WebSocket server logic runs in a separate thread
        WebServer = Async(EAsyncExecution::Thread, [this]() {
            try
            {
	            auto App = uWS::App();
				auto World = this->GetWorld();

				constexpr int32 Port = 59384;           	

				 // Define WebSocket behavior
				uWS::App::WebSocketBehavior<FWebSocketUserData> wsBehavior;

				wsBehavior.compression = uWS::SHARED_COMPRESSOR;
            	
				// Close handler (for when a client disconnects)
				wsBehavior.close = [this](uWS::WebSocket<false, true, FWebSocketUserData>* ws, int code, std::string_view message) {
					ConnectedClients.Remove(ws);
					// TODO: Update log components
					UE_LOG(LogTemp, Log, TEXT("Client Disconnected. Remaining connections: %d"), ConnectedClients.Num());
					OnClientDisconnected(ws, code, message);  // Ensure this signature matches
				};

				// Message handler (for when a client sends a message)
				wsBehavior.message = [this](uWS::WebSocket<false, true, FWebSocketUserData>* ws, std::string_view message, uWS::OpCode opCode) {
					OnMessageReceived(ws, message, opCode);  // Make sure this signature matches
				};

				wsBehavior.open = [this](uWS::WebSocket<false, true, FWebSocketUserData>* ws)
				{
					ConnectedClients.Add(ws);
					// TODO: Update log components
					UE_LOG(LogTemp, Log, TEXT("Client Connected. Connections: %d"), ConnectedClients.Num());
				};
            	
            	App.get("/api/v1/action/regen", [this, World](auto* res, auto* req)
				{

					AsyncTask(ENamedThreads::GameThread, [this]()
					{
						ADeckModSubsystem* ModSubsystem = Get(GetWorld());
						ModSubsystem->ActionIconExtractor();
					});
									
					AddResponseHeaders(res);
					res->end();
					
				});
            	
            	App.get("/api/v1/blueprint/regen", [this, World](auto* res, auto* req)
				{

					AsyncTask(ENamedThreads::GameThread, [this]()
					{
						ADeckModSubsystem* ModSubsystem = Get(GetWorld());
						ModSubsystem->BlueprintIconExtraction_BIE();
					});
					
					AddResponseHeaders(res);
					res->end();
	
				});
            	
            	App.get("/api/v1/customizer/regen", [this, World](auto* res, auto* req)
				{

					AsyncTask(ENamedThreads::GameThread, [this]()
					{
						ADeckModSubsystem* ModSubsystem = Get(GetWorld());
						ModSubsystem->CustomizerIconExtractor();
					});
					
					AddResponseHeaders(res);
					res->end();
	
				});
            	
            	App.get("/api/v1/resource/regen", [this, World](auto* res, auto* req)
				{
					AsyncTask(ENamedThreads::GameThread, [this]()
					{
						ADeckModSubsystem* ModSubsystem = Get(GetWorld());
						ModSubsystem->ResourceIconExtractor();
					});					
									
					AddResponseHeaders(res);
					res->end();
					
				});
            	
            	App.get("/api/v1/recipe/regen", [this, World](auto* res, auto* req)
				{
					AsyncTask(ENamedThreads::GameThread, [this]()
					{
						ADeckModSubsystem* ModSubsystem = Get(GetWorld());
						ModSubsystem->RecipeIconExtraction_BIE();
					});					
					
					AddResponseHeaders(res);
					res->end();
	
				});
            	
            	// List Getters
				App.get("/api/v1/action/list", [this, World](auto* res, auto* req)
				{
					
					TArray<TSharedPtr<FJsonValue>> ActionsJson{};

					for (auto [name, action] : QuickActions)
					{
						TSharedPtr<FJsonObject> JAction = MakeShared<FJsonObject>();  
						JAction->Values.Add("friendlyName", MakeShared<FJsonValueString>(name));
						JAction->Values.Add("descriptor", MakeShared<FJsonValueString>(action));
						ActionsJson.Add(MakeShared<FJsonValueObject>(JAction));
					}   
            						
					AddResponseHeaders(res);

					FString OutputString;
					TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutputString);
					FJsonSerializer::Serialize(ActionsJson, Writer);
            						
					res->end(TCHAR_TO_UTF8(*OutputString));
            						
				});

				App.get("/api/v1/blueprint/list", [this, World](auto* res, auto* req)
				{

					AFGBlueprintSubsystem* BlueprintSubsystem = AFGBlueprintSubsystem::Get(World);
					TArray<UFGBlueprintDescriptor*> BlueprintDescriptors{};
					TArray<TSharedPtr<FJsonValue>> BlueprintDescriptorsJson{};
					BlueprintSubsystem->GetBlueprintDescriptors_Internal(BlueprintDescriptors);

					for (UFGBlueprintDescriptor* BlueprintDescriptor : BlueprintDescriptors)
					{
						TSharedPtr<FJsonObject> JBlueprint = MakeShared<FJsonObject>();
						JBlueprint->Values.Add("friendlyName", MakeShared<FJsonValueString>(BlueprintDescriptor->GetBlueprintNameAsString()));
						JBlueprint->Values.Add("descriptor", MakeShared<FJsonValueString>(BlueprintDescriptor->GetBlueprintNameAsString()));
						BlueprintDescriptorsJson.Add(MakeShared<FJsonValueObject>(JBlueprint));
					}   
            		
					AddResponseHeaders(res);

					FString OutputString;
					TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutputString);
					FJsonSerializer::Serialize(BlueprintDescriptorsJson, Writer);
            		
					res->end(TCHAR_TO_UTF8(*OutputString));
            		
				});

				App.get("/api/v1/recipe/list", [this, World](auto* res, auto* req)
				{
					TSharedRef<FJsonObject> DescriptorsJson = MakeShareable(new FJsonObject());
					TArray<FString> DescriptorKey{};
					TArray<TSharedPtr<FJsonValue>> DescriptorJsonArray;
					Descriptors.GenerateKeyArray(DescriptorKey);
					for (auto [Descriptor, Recipe] : Descriptors)
					{
						TSharedPtr<FJsonObject> DescriptorJson = MakeShared<FJsonObject>();
					
						DescriptorJson->Values.Add("friendlyName", MakeShared<FJsonValueString>(UFGRecipe::GetRecipeName(*Recipe).ToString()));
						DescriptorJson->Values.Add("descriptor", MakeShared<FJsonValueString>(Descriptor));
						DescriptorJsonArray.Add(MakeShared<FJsonValueObject>(DescriptorJson));
					}   

					AddResponseHeaders(res);
					FString OutputString;
					TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutputString);
					FJsonSerializer::Serialize(DescriptorJsonArray, Writer);
            						
					res->end(TCHAR_TO_UTF8(*OutputString));
            										
				});

				App.get("/api/v1/customizer/list", [this, World](auto* res, auto* req)
				{
					TArray<FString> CustomizerDescriptors{};
					TArray<TSharedPtr<FJsonValue>> CustomizerDescriptorJsonArray;
					for (auto [CustomizerDescriptor, Descriptor] : CustomizationDescriptors)
					{
						TSharedPtr<FJsonObject> CustomizerDescriptorJson = MakeShared<FJsonObject>();

						//const TSubclassOf<UFGItemDescriptor> ItemDescriptor{UFGCustomizationRecipe::GetCustomizationDescriptor(Descriptor)};
            			
						CustomizerDescriptorJson->Values.Add("friendlyName", MakeShared<FJsonValueString>(UFGItemDescriptor::GetItemName(Descriptor).ToString()));
						CustomizerDescriptorJson->Values.Add("descriptor", MakeShared<FJsonValueString>(CustomizerDescriptor));
						CustomizerDescriptorJsonArray.Add(MakeShared<FJsonValueObject>(CustomizerDescriptorJson));
					}   

					AddResponseHeaders(res);
					FString OutputString;
					TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutputString);
					FJsonSerializer::Serialize(CustomizerDescriptorJsonArray, Writer);
            		
					res->end(TCHAR_TO_UTF8(*OutputString));
            						
				});
            	
				App.get("/api/v1/resource/list", [this, World](auto* res, auto* req)
				{
					TArray<TSharedPtr<FJsonValue>> ScanDescriptorJsonArray;
            		
            		AFGUnlockSubsystem* UnlockSubsystem = AFGUnlockSubsystem::Get(GetWorld());
					const TArray<FScannableResourcePair> ScanDescriptors = UnlockSubsystem->GetScannableResourcePairs();
            		
					for (auto [ScanDescriptor, ResourceNodeType] : ScanDescriptors)
					{
						TSharedPtr<FJsonObject> CustomizerDescriptorJson = MakeShared<FJsonObject>();
            			
						CustomizerDescriptorJson->Values.Add("friendlyName", MakeShared<FJsonValueString>(UFGItemDescriptor::GetItemName(ScanDescriptor).ToString()));
						CustomizerDescriptorJson->Values.Add("descriptor", MakeShared<FJsonValueString>(UKismetSystemLibrary::GetClassDisplayName(ScanDescriptor)));
						ScanDescriptorJsonArray.Add(MakeShared<FJsonValueObject>(CustomizerDescriptorJson));
					}   

					AddResponseHeaders(res);
					FString OutputString;
					TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutputString);
					FJsonSerializer::Serialize(ScanDescriptorJsonArray, Writer);
            		
					res->end(TCHAR_TO_UTF8(*OutputString));
            						
				});

             	App.get("/api/v1/swatch/list", [this, World](auto* res, auto* req)
				{
					TArray<FString> SwatchDescriptors_Name{};
					TArray<TSharedPtr<FJsonValue>> SwatchDescriptorJsonArray;

            		AFGCharacterPlayer* PlayerCharacter = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerCharacter(World, 0));
					AFGBuildGun* BuildGun = PlayerCharacter->GetBuildGun();
            		
					for (auto [Descriptor_Name, Descriptor] : SwatchDescriptors)
					{
						TSharedPtr<FJsonObject> SwatchDescriptorJson = MakeShared<FJsonObject>();
						FFactoryCustomizationColorSlot ColorSlot{};
						
						SwatchDescriptorJson->Values.Add("friendlyName", MakeShared<FJsonValueString>(UFGItemDescriptor::GetItemName(Descriptor).ToString()));
						SwatchDescriptorJson->Values.Add("descriptor", MakeShared<FJsonValueString>(Descriptor_Name));
						UFGBlueprintFunctionLibrary::GetSlotDataForSwatchDesc(Descriptor, PlayerCharacter, ColorSlot);
						SwatchDescriptorJson->Values.Add("PrimaryColor", MakeShared<FJsonValueString>(UFGBlueprintFunctionLibrary::LinearColorToHex(ColorSlot.PrimaryColor)));
						SwatchDescriptorJson->Values.Add("SecondaryColor", MakeShared<FJsonValueString>(UFGBlueprintFunctionLibrary::LinearColorToHex(ColorSlot.SecondaryColor)));

						SwatchDescriptorJsonArray.Add(MakeShared<FJsonValueObject>(SwatchDescriptorJson));
					}   

					AddResponseHeaders(res);
					FString OutputString;
					TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutputString);
					FJsonSerializer::Serialize(SwatchDescriptorJsonArray, Writer);
            						
					res->end(TCHAR_TO_UTF8(*OutputString));
            										
				});
            	
            	// Actionables i.e. Flashlight, Dismantle, Map, etc.
            	App.get("/api/v1/:action/execute", [World](auto* res, auto* req)
            	{
            		const std::string ActionParam(req->getParameter("action"));
					FString Actionable = FString(ActionParam.c_str());

            		AsyncTask(ENamedThreads::GameThread, [World, Actionable]()
            		{
            			AFGCharacterPlayer* PlayerCharacter = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerCharacter(World, 0));
            			
            			if (Actionable == "flashlight")
            			{
							UE_LOGFMT(LogDeckMod, Log, "Attempting to toggle the flashlight of player: {PlayerCharacter}", PlayerCharacter->GetCachedPlayerName());
							PlayerCharacter->Input_ToggleFlashlight(true);
						} else if (Actionable == "build")
						{
							UE_LOGFMT(LogDeckMod, Log, "Attempting to toggle the build of player: {PlayerCharacter}", PlayerCharacter->GetCachedPlayerName());
							PlayerCharacter->Input_ToggleBuildGunBuild(true); 
						} else if (Actionable == "dismantle")
						{
							UE_LOGFMT(LogDeckMod, Log, "Attempting to toggle the dismantle of player: {PlayerCharacter}", PlayerCharacter->GetCachedPlayerName());
							PlayerCharacter->Input_ToggleBuildGunDismantle(true); 
						} else if (Actionable == "paint")
						{
							UE_LOGFMT(LogDeckMod, Log, "Attempting to toggle the paint of player: {PlayerCharacter}", PlayerCharacter->GetCachedPlayerName());
							PlayerCharacter->Input_ToggleBuildGunPaint(true); 
						} else if (Actionable == "holster")
						{
							UE_LOGFMT(LogDeckMod, Log, "Attempting to toggle the holster of player: {PlayerCharacter}", PlayerCharacter->GetCachedPlayerName());
							PlayerCharacter->Input_Holster(true); 
						} else if (Actionable == "inventory")
						{
							UE_LOGFMT(LogDeckMod, Log, "Attempting to toggle the inventory of player: {PlayerCharacter}", PlayerCharacter->GetCachedPlayerName());
							PlayerCharacter->Input_ToggleInventory(true); 
						} else if (Actionable == "thirdperson")
						{
							UE_LOGFMT(LogDeckMod, Log, "Attempting to toggle the camera of player: {PlayerCharacter}", PlayerCharacter->GetCachedPlayerName());
							PlayerCharacter->ToggleCameraMode();
            			} else if (Actionable == "photomode")
            			{
							UE_LOGFMT(LogDeckMod, Log, "Attempting to toggle the photo mode of player: {PlayerCharacter}", PlayerCharacter->GetCachedPlayerName());
							PlayerCharacter->GetFGPlayerController()->TogglePhotoMode();
						}
            		});

            		AddResponseHeaders(res);
					res->end();
            		
            	});

            	/*
            	
            	## TODO: Resolve issue with interacting with Resource Scanner
            	
            	App.get("/api/v1/:descriptor/scan", [this, World](auto* res, auto* req)
            	{

            		std::string DescriptorParam(req->getParameter("descriptor"));
					FString Descriptor = FString(DescriptorParam.c_str());
            		AsyncTask(ENamedThreads::GameThread, [this, World, Descriptor, res]()
            		{
            			AFGUnlockSubsystem* UnlockSubsystem = AFGUnlockSubsystem::Get(GetWorld());
						const TArray<FScannableResourcePair> ScanDescriptors = UnlockSubsystem->GetScannableResourcePairs();
            			
						for (FScannableResourcePair ScanDescriptor : ScanDescriptors)
						{
							UE_LOGFMT(LogDeckMod, Log, "Scannable Resource Pair: {ScanDescriptor}", UKismetSystemLibrary::GetClassDisplayName(ScanDescriptor.ResourceDescriptor));
							if (Descriptor != UKismetSystemLibrary::GetClassDisplayName(ScanDescriptor.ResourceDescriptor)) continue;
							if (!UnlockSubsystem->IsNodeScannable(ScanDescriptor))
							{
								FString ItemName = UFGItemDescriptor::GetItemName(ScanDescriptor.ResourceDescriptor).ToString();
								UE_LOGFMT(LogDeckMod, Warning, "Node {ScanDescriptor} is not scannable and requires to be unlocked",ItemName );
								SendErrorMessage(res, "400 Bad Request", "Node " + ItemName +" is not scannable and requires to be unlocked");
							};
							
							AFGCharacterPlayer* PlayerCharacter = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerCharacter(World, 0));
							UE_LOGFMT(LogDeckMod, Log, "Scanning for {ScanDescriptor}", ScanDescriptor.ResourceDescriptor->GetClass()->GetName());
							AFGResourceScanner* ResourceScanner = PlayerCharacter->GetResourceScanner();

							PlayerCharacter->EquipEquipment(ResourceScanner);
							ResourceScanner->SetResourceDescriptorToScanFor(ScanDescriptor.ResourceDescriptor);
							ResourceScanner->TriggerDefaultEquipmentActionEvent(EDefaultEquipmentAction::ALL,EDefaultEquipmentActionEvent::Released);							
						}
            		});
            		AddResponseHeaders(res);
            		res->end();
            	});

            	*/
            	
            	App.get("/api/v1/:descriptor/build", [this, World](auto* res, auto* req)
					{
            			std::string DescriptorParam(req->getParameter("descriptor"));
						FString Descriptor = FString(DescriptorParam.c_str());

						AsyncTask(ENamedThreads::GameThread, [this, World, Descriptor, res]()
						{
							AFGCharacterPlayer* PlayerCharacter = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerCharacter(World, 0));
							AFGBuildGun* BuildGun = PlayerCharacter->GetBuildGun();

							if (!Descriptors.Find(Descriptor))
							{
								UE_LOGFMT(LogDeckMod, Warning, "FGBuildingDescriptor {Descriptor} is not found",Descriptor);
								SendErrorMessage(res, "400 Bad Request", "FGBuildingDescriptor " + Descriptor +" is not found");
								return;
							};

							const TSubclassOf<UFGRecipe> Recipe = *Descriptors.Find(Descriptor);
							AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(World);
            			
							if (!RecipeManager->IsRecipeAvailable(Recipe))
							{
								UE_LOGFMT(LogDeckMod, Warning, "FGBuildingDescriptor {Descriptor} is not unlocked",Descriptor);
								SendErrorMessage(res, "400 Bad Request", "FGBuildingDescriptor " + Descriptor +" is not unlocked");
								return;
							};

							if (!PlayerCharacter->IsBuildGunEquipped())
							{
								PlayerCharacter->Input_ToggleBuildGunBuild(true);							
							}
							BuildGun->GotoBuildState(Recipe);
														
						});            		
						
						AddResponseHeaders(res);
						res->end();
					});

            	App.get("/api/v1/:descriptor/blueprint", [this, World](auto* res, auto* req)
				{
					std::string DescriptorParam(req->getParameter("descriptor"));
					FString Descriptor = FString(DescriptorParam.c_str());

					// Change %20 to spaces
					UKismetStringLibrary::ReplaceInline(Descriptor, "%20", " ", ESearchCase::IgnoreCase);
					
					AFGBlueprintSubsystem* BlueprintSubsystem = AFGBlueprintSubsystem::Get(World);
					if (!BlueprintSubsystem->DoesBlueprintExist(Descriptor))
					{
						UE_LOGFMT(LogDeckMod, Warning, "Subsystem - FGBlueprintDescriptor {Descriptor} is not found", Descriptor);
						SendErrorMessage(res, "400 Bad Request", "FGBlueprintDescriptor " + Descriptor +" is not found");
						return;
					};
					UFGBlueprintDescriptor* BlueprintDescriptor = BlueprintSubsystem->GetBlueprintDescriptorByNameString(Descriptor);

            		AsyncTask(ENamedThreads::GameThread, [this, World, Descriptor, BlueprintDescriptor]()
					{

            			AFGCharacterPlayer* PlayerCharacter = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerCharacter(World, 0));
						AFGBuildGun* BuildGun = PlayerCharacter->GetBuildGun();
            		
						if (!PlayerCharacter->IsBuildGunEquipped())
						{
							PlayerCharacter->Input_ToggleBuildGunBuild(true);							
						}
						PlayerCharacter->HotKeyBlueprint(Descriptor);
						
						// This isn't needed? Or is it server?
						//BuildGun->SetDesiredBlueprint(Descriptor);
						
						UFGBuildGunState* buildState = Cast<UFGBuildGunState>(BuildGun->GetCurrentState());
						if (buildState)
							UE_LOGFMT(LogDeckMod, Warning, "Build Gun State is valid");
						
						if (UFGBuildGunStateBuild* buildStateBuild = Cast<UFGBuildGunStateBuild>(buildState))
							buildStateBuild->SetActiveBlueprintDescriptor(BlueprintDescriptor);
						
					});            		
									
					AddResponseHeaders(res);
					res->end();
				});

            	App.get("/api/v1/:descriptor/customizer", [this, World](auto* res, auto* req)
				{
					std::string DescriptorParam(req->getParameter("descriptor"));
					FString Descriptor = FString(DescriptorParam.c_str());

            		AsyncTask(ENamedThreads::GameThread, [this, World, Descriptor, res]()
					{
						AFGCharacterPlayer* PlayerCharacter = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerCharacter(World, 0));
						AFGBuildGun* BuildGun = PlayerCharacter->GetBuildGun();

						if (!CustomizationDescriptors.Find(Descriptor))
						{
							UE_LOGFMT(LogDeckMod, Warning, "UFGCustomizationRecipe {Descriptor} is not found",Descriptor);
							SendErrorMessage(res, "400 Bad Request", "UFGCustomizationRecipe " + Descriptor +" is not found");
							return;
						};

            			TSubclassOf<UFGFactoryCustomizationDescriptor> Recipe = *CustomizationDescriptors.Find(Descriptor);
            			AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(World);
            			if (!Recipe)
            			{
							UE_LOGFMT(LogDeckMod, Warning, "{Descriptor} is not a Customizer Recipe",Descriptor);
							SendErrorMessage(res, "400 Bad Request", Descriptor + " is not a Customizer Recipe");
							return;
						};

            			TSubclassOf<UFGCustomizationRecipe> CustomizationRecipe = RecipeManager->GetCustomizationRecipeFromDesc(Recipe);

						if (!RecipeManager->IsCustomizationRecipeAvailable(CustomizationRecipe))
						{
							UE_LOGFMT(LogDeckMod, Warning, "UFGCustomizationRecipe {Descriptor} is not unlocked",Descriptor);
							SendErrorMessage(res, "400 Bad Request", "UFGCustomizationRecipe " + Descriptor +" is not unlocked");
							return;
						};

            			if (!PlayerCharacter->IsBuildGunEquipped())
            			{
							PlayerCharacter->Input_ToggleBuildGunBuild(true);							
						}
						BuildGun->GotoPaintState(CustomizationRecipe);
					});            		
									
					AddResponseHeaders(res);
					res->end();
				});
            	            	
                // Icon Getter
                App.get("/api/v1/:descriptor/icon", [this, World](auto* res, auto* req) {
                	
                	std::string DescriptorParam(req->getParameter("descriptor"));
					FString Descriptor = FString(DescriptorParam.c_str());
                	
                	// Change %20 to spaces
					UKismetStringLibrary::ReplaceInline(Descriptor, "%20", " ", ESearchCase::IgnoreCase);
                	
                	FString IconsPath = FPaths::ProjectDir() + "SFControlPanelIcons/";
					FString FilePath = FPaths::Combine(IconsPath, Descriptor + ".png");
                	                	
					if (!res || !req) {
						UE_LOG(LogDeckMod, Error, TEXT("Invalid request or response pointer!"));
						return;
					}

					if (FPaths::FileExists(FilePath)) {
						const FString ContentType = "image/png";
						TArray<uint8> BinaryContent;
						
						if (FFileHelper::LoadFileToArray(BinaryContent, *FilePath)) {
							std::string contentLength = std::to_string(BinaryContent.Num());

							res->writeHeader("Content-Type", TCHAR_TO_UTF8(*ContentType));
							AddResponseHeaders(res);
							res->write(std::string_view((char*)BinaryContent.GetData(), BinaryContent.Num()));
							res->end();
						}
					}
					else
					{
						std::string DescriptorUrl(req->getUrl());
						
						UE_LOG(LogDeckMod, Error, TEXT("File not found: %s"), *FilePath);
						UE_LOG(LogDeckMod, Error, TEXT("URL: %s"), *FString(DescriptorUrl.c_str()));
						SendErrorJson(res, "404 Not Found", "");
					}
					
                });

            	// Swatch Getter
				App.get("/api/v1/:descriptor/swatch", [this, World](auto* res, auto* req) {
                					
					std::string DescriptorParam(req->getParameter("descriptor"));
					FString Descriptor = FString(DescriptorParam.c_str());

					AsyncTask(ENamedThreads::GameThread, [this, World, Descriptor, res]()
					{
						AFGCharacterPlayer* PlayerCharacter = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerCharacter(World, 0));
						AFGBuildGun* BuildGun = PlayerCharacter->GetBuildGun();

						if (!SwatchDescriptors.Find(Descriptor))
						{
							UE_LOGFMT(LogDeckMod, Warning, "Swatch {Descriptor} is not found",Descriptor);
							SendErrorMessage(res, "400 Bad Request", "Swatch " + Descriptor +" is not found");
							return;
						};

						TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Recipe = *SwatchDescriptors.Find(Descriptor);
						AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(World);
						if (!Recipe)
						{
							UE_LOGFMT(LogDeckMod, Warning, "{Descriptor} is not a Swatch Recipe",Descriptor);
							SendErrorMessage(res, "400 Bad Request", Descriptor + " is not a Swatch Recipe");
							return;
						};

						TSubclassOf<UFGCustomizationRecipe> CustomizationRecipe = RecipeManager->GetCustomizationRecipeFromDesc(Recipe);

						if (!RecipeManager->IsCustomizationRecipeAvailable(CustomizationRecipe))
						{
							UE_LOGFMT(LogDeckMod, Warning, "UFGCustomizationRecipe {Descriptor} is not unlocked",Descriptor);
							SendErrorMessage(res, "400 Bad Request", "UFGCustomizationRecipe " + Descriptor +" is not unlocked");
							return;
						};

						if (!PlayerCharacter->IsBuildGunEquipped())
						{
							PlayerCharacter->Input_ToggleBuildGunBuild(true);							
						}
						BuildGun->GotoPaintState(CustomizationRecipe);
					});    
					
					AddResponseHeaders(res);
					res->end();
				});
            	
                App.ws<FWebSocketUserData>("/*", std::move(wsBehavior));

            	App.listen("127.0.0.1", Port, [this, Port](us_listen_socket_t* token) {

                    // TODO: Update logging components
					UE_LOG(LogTemp, Warning, TEXT("Attempting to listen on port %d"), Port);

                	FString Reason;
					if (token) {
                        SocketListener = token;
                        UE_LOGFMT(LogTemp, Warning, "Listening on port {port}", Port);

                    	SocketRunning = true;
                    	bShouldStop = false;
                    }
                    else {
                        UE_LOGFMT(LogTemp, Error, "Failed to listen on port {port}", Port);
                    }
                });

                SocketRunning = true;
                
                App.run();

                SocketRunning = false;

                // TODO: Update logging components
				UE_LOG(LogTemp, Log, TEXT("WebSocket Server Thread finished."));
            } catch (const std::exception& e) {
                // TODO: Update logging components
				UE_LOG(LogTemp, Error, TEXT("WebSocket Server Exception: %s"), *FString(e.what()));
            } catch (...) {
                // TODO: Update logging components
				UE_LOG(LogTemp, Error, TEXT("Unknown Exception in WebSocket Server"));
            }
        });

}

void ADeckModSubsystem::AddResponseHeaders(uWS::HttpResponse<false>* res)
{
	res->writeHeader("Connection", "close");
		// uWebSockets does not automatically handle the closing of HTTP connections.
		// Therefore, we instruct the client to close the connection by setting the "Connection" header to "close"
		// instead of using "keep-alive" (default).
}

void ADeckModSubsystem::SendErrorJson(uWS::HttpResponse<false>* res, const FString& Status, const FString& Json)
{
	res->writeStatus(std::string_view(TCHAR_TO_UTF8(*Status)).data());
	AddResponseHeaders(res);

	if (Json.Len() == 0)
	{
		res->end();
		return;
	}

	res->end(TCHAR_TO_UTF8(*Json));
}

void ADeckModSubsystem::SendErrorMessage(uWS::HttpResponse<false>* res, const FString& Status, const FString& Message)
{
	const TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	JsonObject->Values.Add("error", MakeShared<FJsonValueString>(Message));
	SendErrorJson(res, Status, JsonObjectToString(JsonObject));
}

FString ADeckModSubsystem::JsonObjectToString(const TSharedPtr<FJsonObject>& JsonObject) {
	FString OutputString;
	
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutputString);
	if (FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer)) {
		return OutputString;
	}

	return TEXT("Error: Unable to serialize JSON object");
}

std::string UrlDecode(const std::string &Value) {
	std::ostringstream Decoded;
	for (size_t i = 0; i < Value.length(); ++i) {
		if (Value[i] == '%') {
			std::istringstream HexStream(Value.substr(i + 1, 2));
			if (int HexValue; HexStream >> std::hex >> HexValue) {
				Decoded << static_cast<char>(HexValue);
				i += 2;
			} else {
				Decoded << '%'; // Invalid hex sequence
			}
		} else if (Value[i] == '+') {
			Decoded << ' ';
		} else {
			Decoded << Value[i];
		}
	}
	return Decoded.str();
}

std::unordered_map<std::string, std::string> ParseQueryString(const std::string& Query) {
	std::unordered_map<std::string, std::string> QueryPairs;
	std::istringstream QueryStream(Query);
	std::string Pair;
    
	while (std::getline(QueryStream, Pair, '&')) {
		const auto DelimiterPos = Pair.find('=');
		if (DelimiterPos == std::string::npos) continue; // Skip if there's no '=' character

		std::string Key = Pair.substr(0, DelimiterPos);
		std::string Value = Pair.substr(DelimiterPos + 1);
		QueryPairs[UrlDecode(Key)] = UrlDecode(Value);
	}
    
	return QueryPairs;
}

void ADeckModSubsystem::OnClientDisconnected(uWS::WebSocket<false, true, FWebSocketUserData>* ws, int code, std::string_view message) {
    // Remove the client from all endpoint subscriptions
    for (auto& Elem : EndpointSubscribers) {
        Elem.Value.Remove(ws);
    }
}

void ADeckModSubsystem::OnMessageReceived(uWS::WebSocket<false, true, FWebSocketUserData>* ws, std::string_view message, uWS::OpCode opCode) {

	FString MessageContent = FString(message.data()).Left(message.size());

	// Parse JSON message from the client
	TSharedPtr<FJsonObject> JsonRequest;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(MessageContent);

	if (FJsonSerializer::Deserialize(Reader, JsonRequest) && JsonRequest.IsValid())
	{
		this->ProcessClientRequest(ws, JsonRequest);
	}
	else
	{
		// TODO: Update logging components
		UE_LOG(LogTemp, Error, TEXT("Failed to parse client message: %s"), *MessageContent);
	}
}

void ADeckModSubsystem::ProcessClientRequest(uWS::WebSocket<false, true, FWebSocketUserData>* ws, const TSharedPtr<FJsonObject>& JsonRequest)
{
    FString Action = JsonRequest->GetStringField("action");
    const TArray<TSharedPtr<FJsonValue>>* EndpointsArray;
    FString Endpoint;

    if (JsonRequest->TryGetArrayField("endpoints", EndpointsArray))
    {
        for (const TSharedPtr<FJsonValue>& EndpointValue : *EndpointsArray)
        {
            Endpoint = EndpointValue->AsString();

            if (Action == "subscribe")
            {
            	
                if (!EndpointSubscribers.Contains(Endpoint)) {
                    EndpointSubscribers.Add(Endpoint, TSet<uWS::WebSocket<false, true, FWebSocketUserData>*>());
                }

				if (!bHasRunningPushDataLoop) {
					StartWebSocketPushDataLoop();
				}

                EndpointSubscribers[Endpoint].Add(ws);

                // TODO: Update logging components
// UE_LOG(LogTemp, Warning, TEXT("Client subscribed to endpoint: %s"), *Endpoint);
            }
            else if (Action == "unsubscribe" && EndpointSubscribers.Contains(Endpoint))
            {
                EndpointSubscribers[Endpoint].Remove(ws);
                // TODO: Update logging components
// UE_LOG(LogTemp, Warning, TEXT("Client unsubscribed from endpoint: %s"), *Endpoint);
            }
        }
    }
    else if (JsonRequest->TryGetStringField("endpoints", Endpoint)) {

        if (Action == "subscribe")
        {
            if (!EndpointSubscribers.Contains(Endpoint)) {
                EndpointSubscribers.Add(Endpoint, TSet<uWS::WebSocket<false, true, FWebSocketUserData>*>());
            }

			if (!bHasRunningPushDataLoop) {
				StartWebSocketPushDataLoop();
			}

            EndpointSubscribers[Endpoint].Add(ws);

            // TODO: Update logging components
// UE_LOG(LogTemp, Warning, TEXT("Client subscribed to endpoint: %s"), *Endpoint);
        }
        else if (Action == "unsubscribe")
        {
            EndpointSubscribers[Endpoint].Remove(ws);
            // TODO: Update logging components
// UE_LOG(LogTemp, Warning, TEXT("Client unsubscribed from endpoint: %s"), *Endpoint);
        }
    }
}

void ADeckModSubsystem::PushUpdatedData() {

    for (auto& Elem : EndpointSubscribers) {
        FString Endpoint = Elem.Key;
        
        if (Elem.Value.Num() == 0) {
            continue;
        }

        bool bSuccess = false;
    	int32 ErrorCode = 404;

        FString Json;

		// Call updater
    	
    	FTCHARToUTF8 Converted(*Json);
    	const char* UWSOutput = Converted.Get();
    	
        // Broadcast updated data to all clients subscribed to this endpoint
        for (uWS::WebSocket<false, true, FWebSocketUserData>* Client : Elem.Value) {
            Client->send(UWSOutput, uWS::OpCode::TEXT);
        }
    }
}

// Helper function to add error messages to JsonArray
void ADeckModSubsystem::AddErrorJson(TArray<TSharedPtr<FJsonValue>>& JsonArray, const FString& ErrorMessage)
{
    TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
    JsonObject->SetStringField("error", ErrorMessage);
    JsonArray.Add(MakeShared<FJsonValueObject>(JsonObject));
}