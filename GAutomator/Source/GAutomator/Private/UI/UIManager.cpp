#include "UIManager.h"
#include "Layout/Children.h"
#include "EngineUtils.h"
#include "Components/Widget.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine.h"
#include "Common/Log.h"
#include "XmlParser.h"
#include "Common/GXmlJsonTools.h"
#include "Engine/UserInterfaceSettings.h"
#include "TextBlock.h"
#include "RichTextBlock.h"
#include "MultiLineEditableTextBox.h"
#include "Handler/CommandHandler.h"
#include "MultiLineEditableText.h"
#include <ctime> 
#ifdef __ANDROID__
#include "Android/AndroidWindow.h"
#endif

namespace WeTestU3DAutomation
{

	FXmlNode* TransformUmg2XmlElement(UWidget* Widget, FXmlNode* Parent) {

		FXmlNode* WidgetXmlNode = AddFXmlNode(Parent, "UWidget", FString());

		TArray<FXmlAttribute>& Attributes = const_cast<TArray<FXmlAttribute>&>(WidgetXmlNode->GetAttributes());
		UClass* WidgetClass = Widget->GetClass();

		Attributes.Add(FXmlAttribute("name", Widget->GetName()));
		Attributes.Add(FXmlAttribute("components", WidgetClass->GetName()));
		Attributes.Add(FXmlAttribute("id", FString("0")));

		const UTextBlock* TextBlock = Cast<UTextBlock>(Widget);

		if (TextBlock != nullptr)
		{
			Attributes.Add(FXmlAttribute("txt", TextBlock->GetText().ToString()));
		}

		return WidgetXmlNode;
	}

	void ForWidgetAndChildren(UWidget* Widget, FXmlNode* Parent)
	{

		if (Widget == nullptr || Parent == nullptr || !Widget->IsVisible()) {
			return;
		}
		FXmlNode* WidgetXmlNode = TransformUmg2XmlElement(Widget, Parent);
		// Search for any named slot with content that we need to dive into.
		if (INamedSlotInterface* NamedSlotHost = Cast<INamedSlotInterface>(Widget))
		{
			TArray<FName> SlotNames;
			NamedSlotHost->GetSlotNames(SlotNames);

			for (FName SlotName : SlotNames)
			{
				if (UWidget* SlotContent = NamedSlotHost->GetContentForSlot(SlotName))
				{

					ForWidgetAndChildren(SlotContent, WidgetXmlNode);
				}
			}
		}

		// Search standard children.
		if (UPanelWidget* PanelParent = Cast<UPanelWidget>(Widget))
		{
			for (int32 ChildIndex = 0; ChildIndex < PanelParent->GetChildrenCount(); ChildIndex++)
			{
				if (UWidget* ChildWidget = PanelParent->GetChildAt(ChildIndex))
				{
					ForWidgetAndChildren(ChildWidget, WidgetXmlNode);
				}
			}
		}
	}

	FString GetCurrentWidgetTree() {
		TSharedPtr<FXmlFile> xml = CreateFXmlFile();
		FString XmlStr;
		FXmlNode* RootNode = xml->GetRootNode();

		for (TObjectIterator<UUserWidget> Itr; Itr; ++Itr)
		{
			UUserWidget* UserWidget = *Itr;

			if (UserWidget == nullptr || !UserWidget->GetIsVisible() || UserWidget->WidgetTree == nullptr) {
				UE_LOG(GALog, Log, TEXT("UUserWidget Iterator get a null(unvisible) UUserWidget"));
				continue;
			}

			ForWidgetAndChildren(UserWidget->WidgetTree->RootWidget, RootNode);
		}

		WriteNodeHierarchy(*RootNode, FString(), XmlStr);

		return MoveTemp(XmlStr);
	}


	const UWidget* FindUWidgetObject(const FString& name)
	{
		for (TObjectIterator<UUserWidget> Itr; Itr; ++Itr)
		{
			UUserWidget* UserWidget = *Itr;

			if (UserWidget == nullptr || !UserWidget->GetIsVisible() || UserWidget->WidgetTree == nullptr) {
				UE_LOG(GALog, Log, TEXT("UUserWidget Iterator get a null(unvisible) UUserWidget"));
				continue;
			}

			UWidget* Widget = UserWidget->GetWidgetFromName(FName(*name));
			if (Widget != nullptr) {
				return Widget;
			}
		}

		return nullptr;
	}


	void GetElementBound(const FString& name, FBoundInfo& BoundInfo)
	{
		const UWidget* Widget = FindUWidgetObject(name);
	}

	bool GetCurrentLevelName(FString& LevelName)
	{
		for (TObjectIterator<UUserWidget> Itr; Itr; ++Itr)
		{
			UUserWidget* UserWidget = *Itr;

			if (UserWidget == nullptr || !UserWidget->GetIsVisible() || UserWidget->WidgetTree == nullptr) {
				continue;
			}


			UWorld* World = Itr->GetWorld();

			if (World != nullptr)
			{
				LevelName = World->GetMapName();
				return true;
			}
		}

		return false;
	}



	FString GetUWidgetLabelText(const UWidget* Widget)
	{
		if (Widget == nullptr)
		{
			return FString();
		}
		const UMultiLineEditableText* MultiLineEditableText = Cast<UMultiLineEditableText>(Widget);

		if (MultiLineEditableText != nullptr)
		{
			return MultiLineEditableText->GetText().ToString();
		}

		const UTextBlock* TextBlock = Cast<UTextBlock>(Widget);
		if (TextBlock != nullptr)
		{
			return TextBlock->GetText().ToString();
		}

		const UMultiLineEditableTextBox* MultiLineEditableTextBox = Cast<UMultiLineEditableTextBox>(Widget);
		if (MultiLineEditableTextBox != nullptr)
		{
			return MultiLineEditableTextBox->GetText().ToString();
		}

		return FString();
	}

	int32 FUWidgetHelper::SurfaceViewWidth = 0;
	int32 FUWidgetHelper::SurfaceViewHeight = 0;
	float FUWidgetHelper::WidthScale = -1.0f;
	float FUWidgetHelper::HeightScale = -1.0f;
	float FUWidgetHelper::ViewportScale = -1.0f;

	bool FUWidgetHelper::Initialize()
	{
		if (!CheckGEngine())
		{
			UE_LOG(GALog, Error, TEXT("FUWidgetHelper Initialize failed"));
			Inited = false;
			return false;
		}

#ifdef  __ANDROID__
		bool AndroidInitResult = InitScaleByAndroid();
		if (!AndroidInitResult)
		{
			InitViewPortScale();
		}
#else
		//Not Anroid,use general,DPI method.
		InitViewPortScale();
#endif
		Inited = true;
		return true;

	}

	bool FUWidgetHelper::InitViewPortScale()
	{
		if (!CheckGEngine())
		{
			UE_LOG(GALog, Error, TEXT("FUWidgetHelper Initialize failed"));
			Inited = false;
			return false;
		}
		const FVector2D ViewportSize = FVector2D(GEngine->GameViewport->Viewport->GetSizeXY());

		const UUserInterfaceSettings* setting = GetDefault<UUserInterfaceSettings>(UUserInterfaceSettings::StaticClass());
		if (setting != nullptr) {
			ViewportScale = setting->GetDPIScaleBasedOnSize(FIntPoint(ViewportSize.X, ViewportSize.Y));

			if (ViewportScale <= 0.0) {
				UE_LOG(GALog, Error, TEXT("ViewportScale = %f,invaild"), ViewportScale);
				Inited = false;
				return false;
			}
			WidthScale = ViewportScale;
			HeightScale = ViewportScale;
			SurfaceViewWidth = GSystemResolution.ResX / ViewportScale;
			SurfaceViewHeight = GSystemResolution.ResY / ViewportScale;
			UE_LOG(GALog, Log, TEXT("Screen(GSystemResolution) with scale %f, size width= %f,height=%f"), ViewportScale, GSystemResolution.ResX / ViewportScale, GSystemResolution.ResY / ViewportScale);
		}
		return true;
	}

	bool FUWidgetHelper::InitScaleByAndroid()
	{
#ifdef  __ANDROID__
		if (!CheckGEngine())
		{
			UE_LOG(GALog, Error, TEXT("FUWidgetHelper Initialize failed"));
			Inited = false;
			return false;
		}
		const FVector2D ViewportSize = FVector2D(GEngine->GameViewport->Viewport->GetSizeXY());

		if (SurfaceViewWidth != 0.0f&&SurfaceViewHeight != 0.0f) {
			return true;
		}
		void* NativeWindow = FAndroidWindow::GetHardwareWindow();

		FAndroidWindow::CalculateSurfaceSize(NativeWindow, SurfaceViewWidth, SurfaceViewHeight);
		if (SurfaceViewWidth == 0.0f)
		{
			UE_LOG(GALog, Error, TEXT("SurfaceWidth error = 0.0 "));
			SurfaceViewWidth = ViewportSize.X;
		}
		if (SurfaceViewHeight == 0.0f)
		{
			UE_LOG(GALog, Error, TEXT("SurfaceViewHeight error = 0.0 "));
			SurfaceViewHeight = ViewportSize.Y;
		}
		WidthScale = ViewportSize.X / SurfaceViewWidth;
		HeightScale = ViewportSize.Y / SurfaceViewHeight;
		UE_LOG(LogTemp, Log, TEXT("Surfaceview WidthScale=%f ,HeightScale=%f,SurfaceViewWidth = %d,SurfaceViewHeight=%d"), WidthScale, HeightScale, SurfaceViewWidth, SurfaceViewHeight);
		return true;
#else
		return false;
#endif
	}

	bool FUWidgetHelper::CheckGEngine()
	{
		if (GEngine == nullptr || GEngine->GameViewport == nullptr || GEngine->GameViewport->Viewport == nullptr)
		{
			UE_LOG(GALog, Error, TEXT("Global GEngine(GameViewPort) is null"));
			return false;
		}
		return true;
	}


	FVector FUWidgetHelper::GetMobileinfo()
	{
		FVector vector;
		vector.X = ViewportScale;
		vector.Y = SurfaceViewWidth;
		vector.Z = SurfaceViewHeight;
		return vector;
	}

	bool FUWidgetHelper::GetElementBound(const FString& name, FBoundInfo& BoundInfo)
	{
		if (!Inited&&Initialize())
		{
			UE_LOG(GALog, Error, TEXT("Mobile Screen size get error"));
			BoundInfo.existed = false;
			BoundInfo.instance = -1;
			BoundInfo.visible = false;
			return false;
		}

		const UWidget* WidgetPtr = FindUWidgetObject(name);

		if (WidgetPtr == nullptr || !WidgetPtr->IsVisible()) {
			UE_LOG(GALog, Log, TEXT("UObject %s can't find"), *name);
			BoundInfo.existed = false;
			BoundInfo.instance = -1;
			BoundInfo.visible = false;
			return true;
		}

		const FGeometry geometry = WidgetPtr->GetCachedGeometry();

		FVector2D Position = geometry.GetAbsolutePosition();
		FVector2D Size = geometry.GetAbsoluteSize();

		BoundInfo.x = Position.X / WidthScale;
		BoundInfo.y = Position.Y / HeightScale;
		BoundInfo.width = Size.X / WidthScale;
		BoundInfo.height = Size.Y / HeightScale;
		return true;
	}

	bool FUWidgetHelper::PositionInRect(const FGeometry& geometry, float x, float y)
	{
		FVector2D Position = geometry.GetAbsolutePosition();
		FVector2D Size = geometry.GetAbsoluteSize();

		if (x >= Position.X&&y >= Position.Y&&x <= (Position.X + Size.X) && y <= (Position.Y + Size.Y))
		{
			return true;
		}
		return false;
	}

	const UWidget* FUWidgetHelper::FindUWidgetObjectByPos(float x, float y)
	{
		if (!Inited&&Initialize())
		{
			UE_LOG(GALog, Error, TEXT("Mobile Screen size get error"));
			return nullptr;
		}
		float GeometryX = x*WidthScale;
		float GeometryY = y*HeightScale;
		UWidget* ContainPosWidget = nullptr;

		for (TObjectIterator<UUserWidget> Itr; Itr; ++Itr)
		{
			UUserWidget* UserWidget = *Itr;

			if (UserWidget == nullptr || !UserWidget->GetIsVisible() || UserWidget->WidgetTree == nullptr) {
				UE_LOG(GALog, Log, TEXT("UUserWidget Iterator get a null(unvisible) UUserWidget"));
				continue;
			}

			UserWidget->WidgetTree->ForEachWidgetAndDescendants([&ContainPosWidget, this, GeometryX, GeometryY](UWidget* WidgetPtr) {
				if (WidgetPtr == nullptr || !WidgetPtr->IsVisible()) {
					return;
				}
				const FGeometry geometry = WidgetPtr->GetCachedGeometry();

				if (this->PositionInRect(geometry, GeometryX, GeometryY))
				{
					ContainPosWidget = WidgetPtr;
				}

			});
		}

		return ContainPosWidget;
	}

	bool TimeTemp::SetTimerHandle()
	{
		handle = new FTimerHandle();
		UGameInstance* gameInstance = nullptr;
		for (TObjectIterator<UGameInstance> Itr; Itr; ++Itr)
		{
			gameInstance = *Itr;
			if (gameInstance == nullptr)
			{
				continue;
			}
		
			
			timerDel.BindLambda([this]() {TimerHandleFunc(); });
			gameInstance->GetWorld()->GetTimerManager().SetTimer(*handle, timerDel, tickTime, loop);
			return true;
		}
		return false;
	}

	static TArray<FCharacterPos> characterposs;

	void TimeTemp::TimerHandleFunc()
	{
		UGameInstance* gameInstance = nullptr;
		for (TObjectIterator<UGameInstance> Itr; Itr; ++Itr)
		{
			gameInstance = *Itr;
			if (gameInstance == nullptr)
			{
				continue;
			}

			FHitResult Hit, Hit2;
			UE_LOG(GALog, Log, TEXT("Timer Start"));

			//开启射线检测
			ACharacter* character = gameInstance->GetWorld()->GetFirstPlayerController()->GetCharacter();
			FVector vectorStart = character->GetActorLocation();
			vectorStart.Z = 0.0f;
			FVector actorRotator = character->GetActorForwardVector();
			FVector vectorEnd = vectorStart + actorRotator * scales;
			vectorEnd.Z = character->GetDefaultHalfHeight() * 2;
			DrawDebugLine(gameInstance->GetWorld(), vectorStart + FVector(0.0f, 0.0f, 25.0f), vectorEnd, FColor(255, 0, 0), false, 0, 0, 10);
			FCollisionObjectQueryParams checkTrace(ECollisionChannel::ECC_WorldStatic);
			checkTrace.AddObjectTypesToQuery(ECollisionChannel::ECC_PhysicsBody);
			//获取检测结果
			gameInstance->GetWorld()->LineTraceSingleByObjectType(Hit, vectorStart + FVector(0.0f, 0.0f, 25.0f)
				, vectorEnd, FCollisionObjectQueryParams(checkTrace));
			AActor* actor = Hit.GetActor();

			if (actor)
			{
				gameInstance->GetWorld()->GetTimerManager().ClearTimer(*handle);
				handle = nullptr;
				UE_LOG(GALog, Log, TEXT("Disable monitor"));
				FCharacterPos characterpos;
				auto i = reinterpret_cast<std::uintptr_t>(actor);
				characterpos.instance = i;
				characterpos.x = character->GetActorLocation().X;
				characterpos.y = character->GetActorLocation().Y;
				characterpos.z = character->GetActorLocation().Z;
				command.ReponseJsonType = ResponseDataType::OBJECT;
				characterposs.Push(characterpos);
				command.ResponseJson = ArrayToJson<FCharacterPos>(characterposs);
				FCommandHandler::cond_var->notify_one();
			}


		}
	}


	//改变角色旋转角度
	const bool ChangeRotator(const FString& str)
	{
		UGameInstance* gameinstance = nullptr;
		for (TObjectIterator<UGameInstance> Itr; Itr; ++Itr)
		{
			gameinstance = *Itr;

			if (gameinstance == nullptr) {
				continue;
			}

			gameinstance->GetWorld()->GetFirstPlayerController()->GetPawn()->AddControllerYawInput(FCString::Atof(*str));

			return true;
		}

		return false;
	}

	//获取角色当前旋转角度
	const FRotator getRotation()
	{
		FRotator rotator = FRotator(90.0f, 90.0f, 90.0f);
		UGameInstance* gameinstance = nullptr;

		for (TObjectIterator<UGameInstance> Itr; Itr; ++Itr)
		{
			gameinstance = *Itr;

			if (gameinstance == nullptr) {
				continue;
			}

			rotator = gameinstance->GetWorld()->GetFirstPlayerController()->GetPawn()->GetActorRotation();
			break;
		}

		return rotator;
	}


	//获取旋转阈值
	const float getScale()
	{
		UGameInstance* gameinstance = nullptr;
		for (TObjectIterator<UGameInstance> Itr; Itr; ++Itr)
		{
			gameinstance = *Itr;

			if (gameinstance == nullptr) {
				continue;
			}

			return gameinstance->GetWorld()->GetFirstPlayerController()->InputYawScale;
		}
		return 0.0f;
	}


	//获取物体大小
	const FVector getLevelBound(const FString& str)
	{	
		FVector origin = FVector(0, 0, 0);
		FVector boxextent = FVector(0, 0, 0);
		
		for (TObjectIterator<AActor> Itr; Itr; ++Itr)
		{
			AActor* actor = *Itr;

			if (actor == nullptr)
				continue;

			if (actor->GetName() == FString(str))
			{
				actor->GetActorBounds(false, origin, boxextent);
				return boxextent;
			}
		}

		return boxextent;
	}

	//角色向前位移
	const bool setLocation(const FString& str)
	{
		ACharacter* character = nullptr;
		FVector vec = FVector(0, 0, 0);
		UGameInstance* gameinstance = nullptr;

		for (TObjectIterator<UGameInstance> Itr; Itr; ++Itr)
		{
			gameinstance = *Itr;

			if (gameinstance == nullptr)
				continue;

			if (!character)
			{
				character = gameinstance->GetWorld()->GetFirstPlayerController()->GetCharacter();
				vec = gameinstance->GetWorld()->GetFirstPlayerController()->GetPawn()->GetActorForwardVector() * FCString::Atof(*str);
				vec += character->GetTargetLocation();
				UE_LOG(GALog, Log, TEXT("%f,%f,%f"), vec.X, vec.Y, vec.Z);
				if (character->SetActorLocation(vec))
				{
					return true;
					
				}
				else
					return false;
			}
				
		}
		return false;
	}

	//重设角色位置
	const bool setCharacter(float& posx,float& posy)
	{
		ACharacter* character = nullptr;
		FVector vec = FVector(0, 0, 0);
		UGameInstance* gameinstance = nullptr;

		for (TObjectIterator<UGameInstance> Itr; Itr; ++Itr)
		{
			gameinstance = *Itr;

			if (gameinstance == nullptr)
				continue;

			if (!character)
			{
				character = gameinstance->GetWorld()->GetFirstPlayerController()->GetCharacter();
				vec = character->GetTargetLocation();
				vec.X = posx;
				vec.Y = posy;
				UE_LOG(GALog, Log, TEXT("%f,%f,%f"), vec.X, vec.Y, vec.Z);
				if (character->SetActorLocation(vec))
					return true;
				break;

			}

		}
		return false;
	}
	struct FParam
	{
		FString par;
		FString outcome;
	};

	//基于反射调用Api
	const FString callRegisterHandler(FName& funcname, FString& funcparams)
	{
		FParam par;
		par.par = funcparams;
		UClass* ActorRef = FindObject<UClass>((UObject*)ANY_PACKAGE, *FString("MyObject"));
		if (ActorRef)
		{
			UFunction* func = ActorRef->FindFunctionByName(funcname);
			if (func)
			{
				try
				{
					ActorRef->ProcessEvent(func, &par);
					UE_LOG(GALog, Log, TEXT("ProcessEvent Success!"))
					return par.outcome;
				}
				catch (const std::exception& ex)
				{
					UE_LOG(GALog, Log, TEXT("%s"), ex.what())
					return "false";
				}
				
			}
		}
		return "Null";

	}


}