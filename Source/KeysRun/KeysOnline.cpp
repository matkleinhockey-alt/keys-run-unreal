#include "KeysOnline.h"
#include "KeysBoatPawn.h"
#include "KeysData.h"
#include "KeysGeo.h"
#include "KeysMesh.h"
#include "KeysWorld.h"
#include "ProceduralMeshComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"
#include "WebSocketsModule.h"
#include "IWebSocket.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

using namespace KeysGeo;

namespace
{
	constexpr double SendEvery = .1;          // our boat to the server, 10 times a second
	constexpr double FriendTimeout = 6;       // drop a boat we haven't heard about for this long

	const KeysData::FBoatSpec& SpecById(const FString& Id)
	{
		for (const KeysData::FBoatSpec& S : KeysData::Boats()) if (S.Id == Id) return S;
		return KeysData::Boats()[0];
	}

	double AngLerp(double A, double B, double T)
	{
		double D = FMath::Fmod(B - A, 2 * PI);
		if (D > PI) D -= 2 * PI;
		if (D < -PI) D += 2 * PI;
		return A + D * T;
	}

	FString SpeciesName(const FString& Key)
	{
		const KeysData::FSpecies* S = KeysData::Species(Key);
		return S ? S->Name : Key;
	}
}

AKeysOnline::AKeysOnline()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;   // after our boat has moved
	PrimaryActorTick.bTickEvenWhenPaused = true;   // stay connected while you're in the pause menu
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

AKeysOnline* AKeysOnline::Get(const UObject* WorldContext)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) return nullptr;
	TActorIterator<AKeysOnline> It(W);
	return It ? *It : nullptr;
}

void AKeysOnline::BeginPlay()
{
	Super::BeginPlay();
	ServerUrl = TEXT("ws://127.0.0.1:8787");
	GConfig->GetString(TEXT("KeysRun.Online"), TEXT("ServerUrl"), ServerUrl, GGameIni);
	FParse::Value(FCommandLine::Get(), TEXT("KRServer="), ServerUrl);
	FModuleManager::LoadModuleChecked<FWebSocketsModule>(TEXT("WebSockets"));
	// -KRLogin=Name,Crew logs straight in (handy for a desktop shortcut, and for testing)
	FString Login, LName, LCrew;
	if (FParse::Value(FCommandLine::Get(), TEXT("KRLogin="), Login, false) && Login.Split(TEXT(","), &LName, &LCrew)) Connect(LName, LCrew);
}

void AKeysOnline::EndPlay(const EEndPlayReason::Type Reason)
{
	bWant = false;
	if (Socket.IsValid())
	{
		Socket->OnConnected().Clear();
		Socket->OnConnectionError().Clear();
		Socket->OnClosed().Clear();
		Socket->OnMessage().Clear();
		Socket->Close();
		Socket.Reset();
	}
	Super::EndPlay(Reason);
}

// ---------------------------------------------------------------- connection
void AKeysOnline::Connect(const FString& Name, const FString& Crew)
{
	MyName = Name.TrimStartAndEnd().Left(20);
	MyCrew = Crew.TrimStartAndEnd().Left(24);
	if (MyName.IsEmpty() || MyCrew.IsEmpty()) { State = EKeysNet::Failed; Error = TEXT("Enter a name and a crew code."); return; }
	bWant = true;
	Tries = 0;
	if (Socket.IsValid()) { Socket->OnClosed().Clear(); Socket->Close(); Socket.Reset(); }
	Friends.Empty();
	for (UProceduralMeshComponent* M : Meshes) if (M) M->SetVisibility(false);
	for (bool& U : MeshUsed) U = false;
	State = EKeysNet::Connecting;
	Error.Reset();
	UE_LOG(LogTemp, Display, TEXT("Keys Run online: connecting to %s as %s"), *ServerUrl, *MyName);

	Socket = FWebSocketsModule::Get().CreateWebSocket(ServerUrl);
	TWeakObjectPtr<AKeysOnline> WeakThis(this);
	Socket->OnConnected().AddLambda([WeakThis]()
	{
		AKeysOnline* O = WeakThis.Get();
		if (!O) return;
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("t"), TEXT("hello"));
		J->SetStringField(TEXT("name"), O->MyName);
		J->SetStringField(TEXT("crew"), O->MyCrew);
		J->SetNumberField(TEXT("v"), 1);
		if (const AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(O, 0))) J->SetStringField(TEXT("boat"), P->Spec().Id);
		O->Send(J);
	});
	Socket->OnConnectionError().AddLambda([WeakThis](const FString& Err)
	{
		AKeysOnline* O = WeakThis.Get();
		if (!O) return;
		O->State = EKeysNet::Failed;
		O->Error = FString::Printf(TEXT("Can't reach the server (%s)."), *O->ServerUrl);
		O->RetryAt = O->GetWorld()->GetRealTimeSeconds() + FMath::Min(30.0, 3.0 * ++O->Tries);
		UE_LOG(LogTemp, Warning, TEXT("Keys Run online: connection error: %s"), *Err);
	});
	Socket->OnClosed().AddLambda([WeakThis](int32 Code, const FString& Reason, bool bClean)
	{
		AKeysOnline* O = WeakThis.Get();
		if (!O) return;
		const bool bWasOnline = O->State == EKeysNet::Online;
		if (O->State != EKeysNet::Failed) O->State = O->bWant ? EKeysNet::Failed : EKeysNet::Offline;
		if (O->bWant && O->Error.IsEmpty()) O->Error = TEXT("Lost the connection - reconnecting...");
		if (bWasOnline && O->bWant) O->Toasts.Add(TEXT("Lost the online connection - reconnecting..."));
		O->RetryAt = O->GetWorld()->GetRealTimeSeconds() + FMath::Min(30.0, 3.0 * ++O->Tries);
	});
	Socket->OnMessage().AddLambda([WeakThis](const FString& Msg)
	{
		if (AKeysOnline* O = WeakThis.Get()) O->OnMessage(Msg);
	});
	Socket->Connect();
}

void AKeysOnline::Disconnect()
{
	bWant = false;
	if (Socket.IsValid()) { Socket->OnClosed().Clear(); Socket->Close(); Socket.Reset(); }
	State = EKeysNet::Offline;
	Error.Reset();
	Friends.Empty();
	for (UProceduralMeshComponent* M : Meshes) if (M) M->SetVisibility(false);
	for (bool& U : MeshUsed) U = false;
}

FString AKeysOnline::StatusText() const
{
	switch (State)
	{
	case EKeysNet::Connecting: return FString::Printf(TEXT("Connecting as %s..."), *MyName);
	case EKeysNet::Online:
	{
		const int32 N = FriendsOnWater();
		return FString::Printf(TEXT("Online as %s  -  crew \"%s\"  -  %s"), *MyName, *MyCrew,
			N == 0 ? TEXT("no friends on yet") : *FString::Printf(TEXT("%d friend%s on"), N, N == 1 ? TEXT("") : TEXT("s")));
	}
	case EKeysNet::Failed: return Error.IsEmpty() ? FString(TEXT("Offline")) : Error;
	default: return TEXT("Playing solo");
	}
}

int32 AKeysOnline::FriendsOnWater() const
{
	int32 N = 0;
	for (const auto& KV : Friends) if (KV.Key != MyId) N++;
	return N;
}

void AKeysOnline::Send(const TSharedRef<FJsonObject>& O)
{
	if (!Socket.IsValid() || !Socket->IsConnected()) return;
	FString Out;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> W = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(O, W);
	Socket->Send(Out);
}

void AKeysOnline::ReportCatch(const FString& Key, double Weight, const FString& Boat, int64 Score)
{
	if (State != EKeysNet::Online) return;
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("t"), TEXT("catch"));
	J->SetStringField(TEXT("key"), Key);
	J->SetNumberField(TEXT("w"), Weight);
	J->SetStringField(TEXT("boat"), Boat);
	J->SetNumberField(TEXT("score"), double(Score));
	Send(J);
}

void AKeysOnline::ReportScore(int64 Score)
{
	if (State != EKeysNet::Online) return;
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("t"), TEXT("score"));
	J->SetNumberField(TEXT("score"), double(Score));
	Send(J);
}

void AKeysOnline::SendState()
{
	const AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!P) return;
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("t"), TEXT("s"));
	J->SetNumberField(TEXT("x"), FMath::RoundToDouble(P->BoatX() * 100) / 100);
	J->SetNumberField(TEXT("z"), FMath::RoundToDouble(P->BoatZ() * 100) / 100);
	J->SetNumberField(TEXT("h"), FMath::RoundToDouble(P->Heading() * 1000) / 1000);
	J->SetNumberField(TEXT("y"), FMath::RoundToDouble(P->GetActorLocation().Z) / 100);   // the hull as drawn, so friends see it ride the same way
	J->SetNumberField(TEXT("p"), FMath::RoundToDouble(P->GetPitch() * 1000) / 1000);
	J->SetNumberField(TEXT("r"), FMath::RoundToDouble(P->GetRoll() * 1000) / 1000);
	J->SetNumberField(TEXT("v"), FMath::RoundToDouble(P->GetSpeed() * 100) / 100);
	J->SetStringField(TEXT("b"), P->Spec().Id);
	J->SetBoolField(TEXT("on"), P->bRunning);
	Send(J);
}

// ---------------------------------------------------------------- messages from the server
void AKeysOnline::OnMessage(const FString& Msg)
{
	TSharedPtr<FJsonObject> J;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Msg), J) || !J.IsValid()) return;
	const FString T = J->GetStringField(TEXT("t"));
	const double Now = GetWorld()->GetTimeSeconds();

	if (T == TEXT("welcome"))
	{
		MyId = J->GetStringField(TEXT("id"));
		MyName = J->GetStringField(TEXT("name"));
		State = EKeysNet::Online;
		UE_LOG(LogTemp, Display, TEXT("Keys Run online: logged in as %s, crew %s (%s)"), *MyName, *MyCrew, *ServerUrl);
		Error.Reset();
		Tries = 0;
		Toasts.Add(FString::Printf(TEXT("Online as %s in crew \"%s\". Friends with the same crew code will show up here."), *MyName, *MyCrew));
	}
	else if (T == TEXT("error"))
	{
		bWant = false;
		State = EKeysNet::Failed;
		Error = J->GetStringField(TEXT("msg"));
		if (Socket.IsValid()) { Socket->OnClosed().Clear(); Socket->Close(); }
	}
	else if (T == TEXT("peers"))
	{
		TSet<FString> Seen;
		const TArray<TSharedPtr<FJsonValue>>* List;
		if (!J->TryGetArrayField(TEXT("list"), List)) return;
		for (const TSharedPtr<FJsonValue>& V : *List)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid()) continue;
			const FString Id = O->GetStringField(TEXT("id"));
			if (Id == MyId) continue;
			Seen.Add(Id);
			FKeysFriend& F = Friends.FindOrAdd(Id);
			F.Id = Id;
			F.Name = O->GetStringField(TEXT("name"));
			F.Boat = O->GetStringField(TEXT("b"));
			F.X = O->GetNumberField(TEXT("x")); F.Z = O->GetNumberField(TEXT("z")); F.H = O->GetNumberField(TEXT("h"));
			F.Y = O->GetNumberField(TEXT("y")); F.P = O->GetNumberField(TEXT("p")); F.R = O->GetNumberField(TEXT("r"));
			F.V = O->GetNumberField(TEXT("v"));
			F.bOn = O->GetBoolField(TEXT("on"));
			F.HeardT = Now;
			F.bSeen = true;
		}
		// anyone the server no longer lists has left
		for (auto It = Friends.CreateIterator(); It; ++It)
			if (!Seen.Contains(It->Key))
			{
				if (Meshes.IsValidIndex(It->Value.Slot)) { Meshes[It->Value.Slot]->SetVisibility(false); MeshUsed[It->Value.Slot] = false; }
				It.RemoveCurrent();
			}
	}
	else if (T == TEXT("join")) Toasts.Add(J->GetStringField(TEXT("name")) + TEXT(" is on the water."));
	else if (T == TEXT("leave")) Toasts.Add(J->GetStringField(TEXT("name")) + TEXT(" headed in."));
	else if (T == TEXT("catch"))
		Toasts.Add(FString::Printf(TEXT("%s landed a %.1f lb %s!"), *J->GetStringField(TEXT("name")), J->GetNumberField(TEXT("w")), *SpeciesName(J->GetStringField(TEXT("key")))));
	else if (T == TEXT("toast")) Toasts.Add(J->GetStringField(TEXT("msg")));
	else if (T == TEXT("board"))
	{
		CrewCatches.Reset();
		CrewScores.Reset();
		const TArray<TSharedPtr<FJsonValue>>* Arr;
		if (J->TryGetArrayField(TEXT("catches"), Arr))
			for (const TSharedPtr<FJsonValue>& V : *Arr)
				if (const TSharedPtr<FJsonObject> O = V->AsObject())
					CrewCatches.Add({O->GetStringField(TEXT("name")), O->GetStringField(TEXT("key")), O->GetStringField(TEXT("boat")), O->GetStringField(TEXT("date")), O->GetNumberField(TEXT("w"))});
		if (J->TryGetArrayField(TEXT("scores"), Arr))
			for (const TSharedPtr<FJsonValue>& V : *Arr)
				if (const TSharedPtr<FJsonObject> O = V->AsObject())
					CrewScores.Add({O->GetStringField(TEXT("name")), int64(O->GetNumberField(TEXT("score")))});
	}
}

// ---------------------------------------------------------------- every frame
void AKeysOnline::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double T = GetWorld()->GetTimeSeconds();
	const double Real = GetWorld()->GetRealTimeSeconds();
	if (bWant && State == EKeysNet::Failed && Real > RetryAt)
	{
		const int32 Keep = Tries;   // keep backing off: 3 s, 6 s, 9 s ... up to 30 s between tries
		Connect(MyName, MyCrew);
		Tries = Keep;
	}
	if (State == EKeysNet::Online && T - SendT >= SendEvery) { SendT = T; SendState(); }
	UpdateFriends(FMath::Min(double(DeltaSeconds), .1), T);
}

UProceduralMeshComponent* AKeysOnline::MeshFor(FKeysFriend& F)
{
	if (!Meshes.IsValidIndex(F.Slot))
	{
		int32 Free = MeshUsed.IndexOfByKey(false);
		if (Free == INDEX_NONE)
		{
			UProceduralMeshComponent* M = NewObject<UProceduralMeshComponent>(this);
			M->SetupAttachment(Root);
			M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			M->bUseAsyncCooking = true;
			M->RegisterComponent();
			Free = Meshes.Add(M);
			MeshUsed.Add(false);
		}
		MeshUsed[Free] = true;
		F.Slot = Free;
		F.MeshBoat.Reset();
	}
	UProceduralMeshComponent* M = Meshes[F.Slot];
	if (F.MeshBoat != F.Boat)
	{
		FKeysMesh B;
		KeysMesh::BuildBoat(B, SpecById(F.Boat));
		B.Commit(M, 0, AKeysWorld::VertexColorMaterial());
		F.MeshBoat = F.Boat;
	}
	return M;
}

void AKeysOnline::UpdateFriends(double Dt, double T)
{
	for (auto It = Friends.CreateIterator(); It; ++It)
	{
		FKeysFriend& F = It->Value;
		if (T - F.HeardT > FriendTimeout)
		{
			if (Meshes.IsValidIndex(F.Slot)) { Meshes[F.Slot]->SetVisibility(false); MeshUsed[F.Slot] = false; }
			It.RemoveCurrent();
			continue;
		}
		// still on the start screen: nothing to draw yet
		if (!F.bOn)
		{
			if (Meshes.IsValidIndex(F.Slot)) Meshes[F.Slot]->SetVisibility(false);
			F.bPlaced = false;
			continue;
		}
		// carry the last update forward along the heading so the boat doesn't stutter between packets, then ease toward it
		const double Lead = FMath::Min(T - F.HeardT, .4);
		const double TX = F.X - FMath::Sin(F.H) * F.V * Lead, TZ = F.Z - FMath::Cos(F.H) * F.V * Lead;
		if (!F.bPlaced || FMath::Square(TX - F.SX) + FMath::Square(TZ - F.SZ) > 80.0 * 80.0)
		{
			// first sight, or a jump (Miami, respawn): snap
			F.SX = TX; F.SZ = TZ; F.SH = F.H; F.SY = F.Y; F.SP = F.P; F.SR = F.R;
			F.bPlaced = true;
		}
		const double K = FMath::Min(1.0, Dt * 10);
		F.SX += (TX - F.SX) * K; F.SZ += (TZ - F.SZ) * K;
		F.SH = AngLerp(F.SH, F.H, K);
		F.SY += (F.Y - F.SY) * K; F.SP += (F.P - F.SP) * K; F.SR += (F.R - F.SR) * K;

		UProceduralMeshComponent* M = MeshFor(F);
		M->SetVisibility(true);
		const double Fx = -FMath::Sin(F.SH), Fz = -FMath::Cos(F.SH), Rx = FMath::Cos(F.SH), Rz = -FMath::Sin(F.SH);
		const FVector Fw(Fx, Fz, 0), Rt(Rx, Rz, 0), Up(0, 0, 1);
		const FVector Fp = Fw * FMath::Cos(F.SP) + Up * FMath::Sin(F.SP);
		const FVector Rr = Rt * FMath::Cos(F.SR) + Up * FMath::Sin(F.SR);
		M->SetWorldLocationAndRotation(ToUE(F.SX, F.SY, F.SZ), FRotationMatrix::MakeFromXY(Fp, Rr).Rotator());

		// their wake and prop wash, made here so you see them (and can ride them)
		const KeysData::FBoatSpec& S = SpecById(F.Boat);
		F.WakeT += Dt;
		if (F.WakeT > .1 && FMath::Abs(F.V) > 2)
		{
			const double V = FMath::Abs(F.V);
			EmitWake(F.SX - Fx * S.Len * .5, F.SZ - Fz * S.Len * .5, V, S.Len, T, F.WakeT);
			EmitTrail(5000 + F.Slot, F.SX - Fx * S.Len * .48, F.SZ - Fz * S.Len * .48, S.Beam * .5, FMath::Clamp((V - 1.5) / 7, 0.0, 1.0), T);
			F.WakeT = 0;
		}
	}
}

void AKeysOnline::GetTags(TArray<TPair<FVector, FString>>& Out) const
{
	for (const auto& KV : Friends)
	{
		const FKeysFriend& F = KV.Value;
		if (!F.bOn || !F.bPlaced) continue;
		Out.Add(TPair<FVector, FString>(ToUE(F.SX, F.SY + 4.5, F.SZ), F.Name));
	}
}

void AKeysOnline::GetChartDots(TArray<FVector2D>& Out) const
{
	for (const auto& KV : Friends)
		if (KV.Value.bOn && KV.Value.bPlaced) Out.Add(FVector2D(KV.Value.SX, KV.Value.SZ));
}
