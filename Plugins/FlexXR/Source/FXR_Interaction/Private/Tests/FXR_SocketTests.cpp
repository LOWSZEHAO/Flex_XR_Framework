// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Interactable/FXR_Socket.h"
#include "Interactable/FXR_Grab.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

// A socket on something that moves has to keep what is seated in it. These run in a real world with
// physics, because attachment and welding are what is under test, but the socket is ticked by hand
// rather than the world, so nothing falls or drifts between checks.

namespace FXR_SocketTest
{
	UStaticMeshComponent* SpawnMesh(UWorld* World, const TCHAR* MeshPath, const FVector& Location, float Scale, EComponentMobility::Type Mobility)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Actor);
		Mesh->SetMobility(Mobility);
		Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, MeshPath));
		Mesh->SetWorldLocation(Location);
		Mesh->SetWorldScale3D(FVector(Scale));
		Actor->SetRootComponent(Mesh);
		Mesh->RegisterComponent();
		return Mesh;
	}

	// A cube with collision, standing in for a gun body or a magazine.
	UStaticMeshComponent* SpawnBlock(UWorld* World, const FVector& Location, float Scale, EComponentMobility::Type Mobility)
	{
		return SpawnMesh(World, TEXT("/Engine/BasicShapes/Cube.Cube"), Location, Scale, Mobility);
	}

	template <typename T>
	T* AddComponent(UStaticMeshComponent* Mesh, const FVector& RelativeLocation = FVector::ZeroVector)
	{
		T* Component = NewObject<T>(Mesh->GetOwner());
		Component->SetupAttachment(Mesh);
		Component->SetRelativeLocation(RelativeLocation);
		Component->RegisterComponent();
		return Component;
	}

	// Long enough to finish any seat slide in one step.
	constexpr float SettleTime = 1.f;

	// The socket's default Socket Radius.
	constexpr float DefaultRadius = 15.f;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFXR_SocketFollowsMovingMountTest,
	"FlexXR.Interaction.Socket.FollowsMovingMount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFXR_SocketFollowsMovingMountTest::RunTest(const FString& Parameters)
{
	using namespace FXR_SocketTest;

	FTestWorldWrapper Wrapper;
	if (!Wrapper.CreateTestWorld(EWorldType::Game) || !Wrapper.BeginPlayInTestWorld())
	{
		Wrapper.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();

	// The gun is kinematic, the way it is while a hand holds it. The magazine simulates, as if it had
	// been lying on a table.
	UStaticMeshComponent* Gun = SpawnBlock(World, FVector::ZeroVector, 1.f, EComponentMobility::Movable);
	UFXR_Socket* Socket = AddComponent<UFXR_Socket>(Gun, FVector(0.f, 0.f, -60.f));
	UStaticMeshComponent* Magazine = SpawnBlock(World, FVector(200.f, 0.f, 0.f), 0.2f, EComponentMobility::Movable);
	Magazine->SetSimulatePhysics(true);
	UFXR_Grab* Grab = AddComponent<UFXR_Grab>(Magazine);

	if (!TestTrue(TEXT("Magazine simulates before it is seated"), Magazine->IsSimulatingPhysics()))
	{
		return false;
	}

	// Move the gun halfway through the slide. It has to finish on the socket where it is now, not
	// where it was when the slide started.
	Socket->Seat(Grab);
	Socket->TickComponent(0.1f, LEVELTICK_All, nullptr);
	Gun->SetWorldLocationAndRotation(FVector(0.f, 300.f, 50.f), FRotator(0.f, 90.f, 0.f));
	Socket->TickComponent(SettleTime, LEVELTICK_All, nullptr);

	TestTrue(TEXT("Seated"), Socket->IsOccupied());
	TestFalse(TEXT("Parked while seated"), Magazine->IsSimulatingPhysics());
	TestTrue(TEXT("Slide finishes on the moved socket"), Magazine->GetComponentLocation().Equals(Socket->GetComponentLocation(), 0.1f));
	TestTrue(TEXT("Welded into the gun once settled"), Magazine->IsWelded());
	TestTrue(TEXT("Carried by the gun mesh"), Magazine->GetAttachParent() == Gun);

	// Carry the gun somewhere else entirely, turned as well as moved.
	Gun->SetWorldLocationAndRotation(FVector(-150.f, 40.f, 120.f), FRotator(30.f, -45.f, 10.f));
	TestTrue(TEXT("Follows the gun"), Magazine->GetComponentLocation().Equals(Socket->GetComponentLocation(), 0.1f));
	TestTrue(TEXT("Keeps the socket's facing"), Magazine->GetComponentQuat().Equals(Socket->GetComponentQuat(), 1.e-3f));
	TestTrue(TEXT("Keeps its own scale"), Magazine->GetComponentScale().Equals(FVector(0.2f), 1.e-3f));

	// Ejecting hands it back the way it came: loose, unwelded and simulating.
	Socket->Eject();
	TestNull(TEXT("Detached on eject"), Magazine->GetAttachParent());
	TestFalse(TEXT("Unwelded on eject"), Magazine->IsWelded());
	TestTrue(TEXT("Simulating again after eject"), Magazine->IsSimulatingPhysics());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFXR_SocketStaticMountTest,
	"FlexXR.Interaction.Socket.StaticMountDoesNotWeld",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFXR_SocketStaticMountTest::RunTest(const FString& Parameters)
{
	using namespace FXR_SocketTest;

	FTestWorldWrapper Wrapper;
	if (!Wrapper.CreateTestWorld(EWorldType::Game) || !Wrapper.BeginPlayInTestWorld())
	{
		Wrapper.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();

	// A wall mount never moves, so the object only needs to hang on the socket.
	UStaticMeshComponent* Wall = SpawnBlock(World, FVector::ZeroVector, 1.f, EComponentMobility::Static);
	UFXR_Socket* Socket = AddComponent<UFXR_Socket>(Wall, FVector(60.f, 0.f, 0.f));
	UStaticMeshComponent* Extinguisher = SpawnBlock(World, FVector(0.f, 200.f, 0.f), 0.3f, EComponentMobility::Movable);
	Extinguisher->SetSimulatePhysics(true);
	UFXR_Grab* Grab = AddComponent<UFXR_Grab>(Extinguisher);

	Socket->Seat(Grab);
	Socket->TickComponent(SettleTime, LEVELTICK_All, nullptr);

	TestTrue(TEXT("Seated on the socket"), Extinguisher->GetComponentLocation().Equals(Socket->GetComponentLocation(), 0.1f));
	TestFalse(TEXT("Not welded to a static mount"), Extinguisher->IsWelded());
	TestTrue(TEXT("Hangs on the socket itself"), Extinguisher->GetAttachParent() == Socket);

	Socket->Eject();
	TestNull(TEXT("Detached on eject"), Extinguisher->GetAttachParent());
	TestTrue(TEXT("Simulating again after eject"), Extinguisher->IsSimulatingPhysics());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFXR_SocketOffPivotTest,
	"FlexXR.Interaction.Socket.SnapsByCentreNotPivot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFXR_SocketOffPivotTest::RunTest(const FString& Parameters)
{
	using namespace FXR_SocketTest;

	FTestWorldWrapper Wrapper;
	if (!Wrapper.CreateTestWorld(EWorldType::Game) || !Wrapper.BeginPlayInTestWorld())
	{
		Wrapper.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();

	UStaticMeshComponent* Wall = SpawnBlock(World, FVector(0.f, 0.f, -100.f), 1.f, EComponentMobility::Static);
	UFXR_Socket* Socket = AddComponent<UFXR_Socket>(Wall, FVector(0.f, 0.f, 100.f));

	// A part with its pivot well off the mesh, like the parts in a modular gun kit. The engine's
	// material preview mesh has its pivot at the bottom and its centre high above it.
	UStaticMeshComponent* Part = SpawnMesh(World, TEXT("/Engine/EngineMeshes/SM_MatPreviewMesh_01.SM_MatPreviewMesh_01"),
		FVector(300.f, 0.f, 0.f), 0.1f, EComponentMobility::Movable);
	UFXR_Grab* Grab = AddComponent<UFXR_Grab>(Part);

	const FVector LocalCentre = Part->GetComponentTransform().InverseTransformPosition(Part->Bounds.Origin);
	const FVector CentreOffset = LocalCentre * Part->GetComponentScale();
	if (!TestTrue(TEXT("Test mesh has its pivot well off its centre"), CentreOffset.Size() > 10.f))
	{
		return false;
	}
	const FVector SeatedCentre = Socket->GetComponentLocation() + Socket->GetComponentQuat().RotateVector(CentreOffset);

	// Held nearly upside down with the part itself right where it will sit. The pivot is now well
	// outside the radius, but what you can see is in place, so it has to be accepted.
	const FQuat Turned(FRotator(0.f, 0.f, 170.f));
	const FVector PivotForCentre = SeatedCentre - Turned.RotateVector(CentreOffset);
	Part->SetWorldLocationAndRotation(PivotForCentre, Turned);
	TestTrue(TEXT("Pivot is outside the radius"), FVector::Dist(Part->GetComponentLocation(), Socket->GetComponentLocation()) > DefaultRadius);

	float DistanceSq = MAX_flt;
	TestTrue(TEXT("Accepted by where the part is"), Socket->CanAccept(Grab, DistanceSq));
	TestTrue(TEXT("Measured from the centre"), DistanceSq < 1.f);

	// The reverse: pivot right on the socket but the part hanging the wrong way, so nothing you can
	// see is anywhere near the seat.
	Part->SetWorldLocationAndRotation(Socket->GetComponentLocation(), Turned);
	TestFalse(TEXT("Rejected when only the pivot is in place"), Socket->CanAccept(Grab, DistanceSq));

	// Seat from the first pose. The centre is already in place, so it should stay put while the part
	// turns upright around it, not swing round the pivot.
	Part->SetWorldLocationAndRotation(PivotForCentre, Turned);
	Socket->Seat(Grab);
	Socket->TickComponent(0.1f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Still turning mid-slide"), Part->GetComponentQuat().Equals(Socket->GetComponentQuat(), 1.e-3f));
	TestTrue(TEXT("Centre holds still mid-slide"), Part->Bounds.Origin.Equals(SeatedCentre, 0.1f));

	Socket->TickComponent(SettleTime, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Pivot lands on the socket"), Part->GetComponentLocation().Equals(Socket->GetComponentLocation(), 0.1f));
	TestTrue(TEXT("Upright once seated"), Part->GetComponentQuat().Equals(Socket->GetComponentQuat(), 1.e-3f));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
