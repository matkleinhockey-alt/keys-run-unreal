#include "KeysGameMode.h"
#include "KeysVisitors.h"
#include "KeysLocals.h"
#include "KeysBoatPawn.h"
#include "KeysHUD.h"
#include "KeysWorld.h"
#include "KeysTraffic.h"
#include "KeysExtras.h"
#include "KeysAudio.h"
#include "KeysLife.h"
#include "KeysOnline.h"
#include "KeysGeo.h"
#include "KeysSeafloor.h"
#include "KeysDiver.h"
#include "KeysReef.h"
#include "KeysTown.h"
#include "KeysWalker.h"
#include "KeysLandmarks.h"
#include "KeysEvents.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"

AKeysGameMode::AKeysGameMode()
{
	DefaultPawnClass = AKeysBoatPawn::StaticClass();
	HUDClass = AKeysHUD::StaticClass();
}

template <class T> static T* SpawnOnce(UWorld* W)
{
	TActorIterator<T> It(W);
	if (It) return *It;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return W->SpawnActor<T>(T::StaticClass(), FTransform::Identity, P);
}

void AKeysGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	KeysGeo::Init();
	UWorld* W = GetWorld();
	if (!W) return;
	const bool bHave = (bool)TActorIterator<AKeysWorld>(W);
	if (!bHave)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		W->SpawnActor<AKeysWorld>(AKeysWorld::StaticClass(), FTransform::Identity, P);
	}
	SpawnOnce<AKeysSeafloor>(W);
	SpawnOnce<AKeysDiver>(W);
	SpawnOnce<AKeysReef>(W);
	SpawnOnce<AKeysTown>(W);
	SpawnOnce<AKeysWalker>(W);
	SpawnOnce<AKeysLandmarks>(W);
	SpawnOnce<AKeysLife>(W);
	SpawnOnce<AKeysTraffic>(W);
	SpawnOnce<AKeysOnline>(W);
	SpawnOnce<AKeysEvents>(W);
	SpawnOnce<AKeysVisitors>(W);
	SpawnOnce<AKeysLocals>(W);
	AKeysExtras* Ex = SpawnOnce<AKeysExtras>(W);
	AKeysAudio* Au = SpawnOnce<AKeysAudio>(W);
	if (Ex && Au)
	{
		// the buddy boat, Luigi mode and Caelen make noise through the audio actor
		TWeakObjectPtr<AKeysAudio> WA(Au);
		Ex->OnSound.AddLambda([WA](FName Id, FVector Pos, float Power)
		{
			AKeysAudio* A = WA.Get();
			if (!A) return;
			if (Id == TEXT("Splash")) A->Splash(Pos.X, Pos.Z, 12, Power);
			else A->PlayExtra(Id, Pos, Power);
		});
	}
}

void AKeysGameMode::RestartPlayer(AController* NewPlayer)
{
	// the level has no PlayerStart; the boat places itself in Boot Key Harbor on BeginPlay
	if (!NewPlayer || NewPlayer->IsPendingKillPending()) return;
	RestartPlayerAtTransform(NewPlayer, FTransform(FVector(0, 0, 500)));
}
