#pragma once


#include "FGFactoryColoringTypes.h"
#include "FGRecipe.h"
#include "Unlocks/FGUnlockScannableResource.h"
#include "Subsystem/ModSubsystem.h"
#include "Containers/Map.h"

THIRD_PARTY_INCLUDES_START
#include "ThirdParty/uWebSockets/App.h"
THIRD_PARTY_INCLUDES_END

#include "SFControlPanelSubsystem.generated.h"

struct FWebSocketUserData {
	// Add any fields here you want to track for each WebSocket client
	int32 ClientID{};
	FString ClientName{};
};

struct FClientInfo
{
	FString SubscribedEndpoints{};  // Keep track of all endpoints that have been subscribed
	TArray<uWS::WebSocket<false, true, FWebSocketUserData>*> Client{};  // Add the third template argument for USERDATA
};

UCLASS()
class SFCONTROLPANEL_API ADeckModSubsystem : public AModSubsystem
{
	GENERATED_BODY()

private:

	TFuture<void> WebServer{};
	
	bool bShouldStop = false;
	bool bHasRunningPushDataLoop = false;
	
protected:
	
	friend class AFGPlayerController;
	friend class AFGResourceScanner;

public:

	ADeckModSubsystem();
	virtual ~ADeckModSubsystem() override;

	/** Get the subsystem in the current world, can be nullptr, e.g., on game ending (destroy) or game startup. */
	static ADeckModSubsystem* Get(UWorld* world);

	TMap<FString, TSet<uWS::WebSocket<false, true, FWebSocketUserData>*>> EndpointSubscribers{};

	TSet<uWS::WebSocket<false, true, FWebSocketUserData>*> ConnectedClients{};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Deck Mod")
	TMap<FString, TSubclassOf<UFGRecipe>> Descriptors{};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Deck Mod")
	TMap<FString, TSubclassOf<UFGFactoryCustomizationDescriptor>> CustomizationDescriptors{};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Deck Mod")
	TMap<FString, TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch>> SwatchDescriptors{};
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Deck Mod")
	TMap<FString, FString> QuickActions{};
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Deck Mod")
	void ActionIconExtractor();

	UFUNCTION(BlueprintImplementableEvent, Category = "Deck Mod")
	void BlueprintIconExtraction_BIE();

	UFUNCTION(BlueprintImplementableEvent, Category = "Deck Mod")
	void CustomizerIconExtractor();
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Deck Mod")
	void RecipeIconExtraction_BIE();
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Deck Mod")
	void ResourceIconExtractor();	

	void StartWebSocketServer(bool bSkipIfRunning = false);
	void StopWebSocketServer();
	void GenerateRecipes();

	void OnClientDisconnected(uWS::WebSocket<false, true, FWebSocketUserData>* ws, int code, std::string_view message);
	void OnMessageReceived(uWS::WebSocket<false, true, FWebSocketUserData>* ws, std::string_view message, uWS::OpCode opCode);
	void ProcessClientRequest(uWS::WebSocket<false, true, FWebSocketUserData>* ws, const TSharedPtr<FJsonObject>& JsonRequest);

	void PushUpdatedData();
	
	static FString JsonObjectToString(const TSharedPtr<FJsonObject>& JsonObject);
	
	static void SendErrorJson(uWS::HttpResponse<false>* res, const FString& Status, const FString& Json);
	static void SendErrorMessage(uWS::HttpResponse<false>* res, const FString& Status, const FString& Message);

	static void AddResponseHeaders(uWS::HttpResponse<false>* res);	
	static void AddErrorJson(TArray<TSharedPtr<FJsonValue>>& JsonArray, const FString& ErrorMessage);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void StartWebSocketPushDataLoop();

};