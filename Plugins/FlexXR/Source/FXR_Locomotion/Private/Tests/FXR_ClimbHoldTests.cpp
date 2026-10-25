// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "World/FXR_ClimbHold.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFXR_ClimbHoldReachTest,
	"FlexXR.Locomotion.ClimbHold.ReachFollowsTheHold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFXR_ClimbHoldReachTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!Wrapper.CreateTestWorld(EWorldType::Game) || !Wrapper.BeginPlayInTestWorld())
	{
		Wrapper.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();

	// A ladder with its pivot at the bottom and a hold moved up to a rung.
	AActor* Ladder = World->SpawnActor<AActor>();
	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Ladder);
	Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Ladder->SetRootComponent(Mesh);
	Mesh->RegisterComponent();

	UFXR_ClimbHold* Hold = NewObject<UFXR_ClimbHold>(Ladder);
	Hold->SetupAttachment(Mesh);
	Hold->SetRelativeLocation(FVector(0.f, 0.f, 145.f));
	Hold->RegisterComponent();

	TestTrue(TEXT("Reach is centred on the hold, not the ladder's pivot"),
		Hold->GetInteractionLocation().Equals(Hold->GetComponentLocation(), 0.01f));

	// A hand at the rung reaches it; a hand down at the ladder's pivot no longer does.
	float DistanceSq = 0.f;
	TestTrue(TEXT("A hand at the rung reaches the hold"),
		Hold->IsInGrabReach(Hold->GetComponentLocation(), 5.f, EFXR_HandSide::Right, DistanceSq));
	TestFalse(TEXT("A hand at the ladder's pivot does not"),
		Hold->IsInGrabReach(Mesh->GetComponentLocation(), 5.f, EFXR_HandSide::Right, DistanceSq));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
