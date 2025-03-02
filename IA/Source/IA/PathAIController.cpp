// Fill out your copyright notice in the Description page of Project Settings.

#include "PathAIController.h"
#include "GenericPlatformMath.h"
#include "Engine/World.h"
#include "Navigation/PathFollowingComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/PlayerController.h"

struct Map {
	float weight;
	FVector2D position;
};

struct MapOperator
{
	bool operator()(const Map& A, const Map& B) const
	{
		return A.weight < B.weight;
	}
};

//x
#define MAP_WIDTH 46
//y
#define MAP_HEIGHT 46
#define MAP_BOX_SIZE 100

#define MID_SIZE 2600.f

void APathAIController::BeginPlay()
{
	Super::BeginPlay();
	if (GetControlledShooter())
	{
		AStar();
	}
}

ATwinStickShooter* APathAIController::GetControlledShooter() const
{
	return Cast<ATwinStickShooter>(GetPawn());
}

void APathAIController::AStar()
{
	ControlledPawn = GetControlledShooter();
	Dest = ControlledPawn->GetActorLocation();
	PlayerLocation = GetWorld()->GetFirstPlayerController()->GetPawn()->GetActorLocation();
	PlayerLocationIndex = WorldCordinatesToMapIndex(FVector2D(PlayerLocation.X, PlayerLocation.Y));
	DestIndex = WorldCordinatesToMapIndex(FVector2D(Dest.X, Dest.Y));

	TArray<Map>open;
	TArray<FVector2D>closed;
	TArray<TArray<float>>mapG;

	//Initializing the arrays
	TArray<FVector2D> templates;
	templates.Init({ 0,0 }, MAP_WIDTH);
	Parentmap.Init(templates, MAP_HEIGHT);
	TArray<float> t;
	t.Init(0, MAP_WIDTH);
	mapG.Init(t, MAP_HEIGHT);
	t.Init(-1, MAP_WIDTH);
	map.Init(t, MAP_HEIGHT);

	//Begin Astar
	open.HeapPush({ Manhattan(PlayerLocationIndex), PlayerLocationIndex },MapOperator());
	while (open.Num()!=0) 
	{
		Map aux; open.HeapPop(aux, MapOperator(), false);
		if (aux.position == DestIndex) break;
		for (int32 i = 0; i < MoveDirections.Num(); i++) 
		{
			FVector2D newPosition = aux.position + MoveDirections[i];
			if (CheckMap(newPosition) /*&& CheckWalls(aux.position)*/ && CheckEdges(aux.position,MoveDirections[i]))
			{
				float G = FPlatformMath::Abs(MoveDirections[i].X)==FPlatformMath::Abs(MoveDirections[i].Y)? mapG[aux.position.Y][aux.position.X] + (MAP_BOX_SIZE*1.4): mapG[aux.position.Y][aux.position.X] + MAP_BOX_SIZE;
				float F = G + Manhattan(newPosition);
				if ((map[newPosition.Y][newPosition.X] == -1 || map[newPosition.Y][newPosition.X] > F) && closed.Find(newPosition))
				{
					Parentmap[newPosition.Y][newPosition.X] = aux.position;
					map[newPosition.Y][newPosition.X] = F;
					mapG[newPosition.Y][newPosition.X] = G;
					if (CheckWalls(newPosition))
					{
						open.HeapPush(Map({ F, newPosition }), MapOperator());
					}
				}
			}
		}
		closed.Push(aux.position);
	}	
	Parentmap[PlayerLocationIndex.Y][PlayerLocationIndex.X] = { 0,0 };
	map[DestIndex.Y][DestIndex.X] = 0;
	pathFinder(Parentmap[DestIndex.Y][DestIndex.X]);
}

FVector2D APathAIController::WorldCordinatesToMapIndex(FVector2D WorldLocation)
{
		int x = FPlatformMath::CeilToInt((WorldLocation.X + MID_SIZE) / MAP_BOX_SIZE);
		int y = FPlatformMath::CeilToInt((WorldLocation.Y + MID_SIZE) / MAP_BOX_SIZE);
		return FVector2D(x, y);
}

FVector APathAIController::MapIndexToWorldLocation(FVector2D MapIndex)
{
	if (CheckMap(MapIndex))
	{
		float WX = MapIndex.X*MAP_BOX_SIZE + (MAP_BOX_SIZE / 2) - MID_SIZE;
		float WY = MapIndex.Y*MAP_BOX_SIZE + (MAP_BOX_SIZE / 2) - MID_SIZE;
		return FVector(WX, WY, ControlledPawn->GetActorLocation().Z);
	}
	return FVector();
}

bool APathAIController::CheckMap(FVector2D V)
{
	return V.X >= 0 && V.X < MAP_WIDTH && V.Y >= 0 && V.Y < MAP_HEIGHT;
}

float APathAIController::Manhattan(FVector2D _Dest)
{
	FVector2D DestIndex = WorldCordinatesToMapIndex(FVector2D(Dest.X, Dest.Y));
	if (CheckMap(DestIndex))
	{
		float r = (FPlatformMath::Abs(_Dest.X - Dest.X) + FPlatformMath::Abs(_Dest.Y - Dest.Y)) * MAP_BOX_SIZE;
		return r;
	}
	return -1.f;
}

void APathAIController::pathFinder(FVector2D position)
{
	Path.Empty();
	Path.Push(Dest);
	TArray<FVector2D>s; s.Push(position);
	while (true)
	{
		auto p = s.Top(); s.Pop();
		if (p == FVector2D::ZeroVector) break;
		Path.Push(MapIndexToWorldLocation(p));
		map[p.Y][p.X] = 0;
		s.Push(Parentmap[p.Y][p.X]);
	}
	if (Path.Num() > 3)
	{
		Path.RemoveAt(1);
	}
	if (Path.Num() > 1)
	{
		Path.RemoveAt(Path.Num() - 1);
		Path[Path.Num() - 1] = PlayerLocation;
	}
}

bool APathAIController::CheckWalls(FVector2D position)
{
	if (map[position.Y][position.X] == -2) 
	{
		return true;
	}
	FHitResult HitResult1, HitResult2, HitResult3, HitResult4;
	GetWorld()->LineTraceSingleByChannel(HitResult1, MapIndexToWorldLocation(position - (1, 0)), MapIndexToWorldLocation(position + (1, 0)), ECollisionChannel::ECC_WorldStatic);
	GetWorld()->LineTraceSingleByChannel(HitResult2, MapIndexToWorldLocation(position - (0, 1)), MapIndexToWorldLocation(position + (0, 1)), ECollisionChannel::ECC_WorldStatic);
	GetWorld()->LineTraceSingleByChannel(HitResult3, MapIndexToWorldLocation(position - (1, 1)), MapIndexToWorldLocation(position + (1, 1)), ECollisionChannel::ECC_WorldStatic);
	GetWorld()->LineTraceSingleByChannel(HitResult4, MapIndexToWorldLocation(position - (1, -1)), MapIndexToWorldLocation(position + (1, -1)), ECollisionChannel::ECC_WorldStatic);
	if (!(!HitResult1.GetActor() && !HitResult2.GetActor() && !HitResult3.GetActor() && !HitResult3.GetActor() && !HitResult4.GetActor()))
	{
		map[position.Y][position.X] = -2;
	}
	return !HitResult1.GetActor() && !HitResult2.GetActor() && !HitResult3.GetActor() && !HitResult3.GetActor() && !HitResult4.GetActor();
}

bool APathAIController::CheckEdges(FVector2D position, FVector2D movedir)
{
	if (!CheckMap(position)|| !CheckMap(position - movedir))return false;
	if (FPlatformMath::Abs(movedir.X) != FPlatformMath::Abs(movedir.Y))return true;
	return map[position.Y - movedir.Y][position.X] != -2 && map[position.Y][position.X - movedir.X] != -2;
}

void APathAIController::Movement(float DeltaTime)
{
	if (Path.Num() > 1)
	{
		auto MyLocation = GetControlledShooter()->GetActorLocation();
		FVector MyLocationRounded = FVector(FPlatformMath::RoundToInt(MyLocation.X), FPlatformMath::RoundToInt(MyLocation.Y), FPlatformMath::RoundToInt(MyLocation.Z));
		
		if (WorldCordinatesToMapIndex(FVector2D(MyLocationRounded.X, MyLocationRounded.Y)) == WorldCordinatesToMapIndex(FVector2D(Path[1].X, Path[1].Y)))
		{
			Path.RemoveAt(1);
		}
		FVector Dir = -MyLocation + Path[1];
		Dir.Normalize();
		GetControlledShooter()->Movement(Dir,DeltaTime);
	}
}

//76*76 array map from -1900 to 1900
void APathAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (PlayerLocation != GetWorld()->GetFirstPlayerController()->GetPawn()->GetActorLocation())
	{
		AStar();
	}
	Movement(DeltaTime);
}
