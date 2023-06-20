/*
 * Copyright 2023 Magnopus LLC

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "AssetSystemTestHelpers.h"
#include "Awaitable.h"
#include "CSP/CSPFoundation.h"
#include "CSP/Common/Optional.h"
#include "CSP/Multiplayer/Components/AnimatedModelSpaceComponent.h"
#include "CSP/Multiplayer/Components/AudioSpaceComponent.h"
#include "CSP/Multiplayer/Components/CollisionSpaceComponent.h"
#include "CSP/Multiplayer/Components/CustomSpaceComponent.h"
#include "CSP/Multiplayer/Components/ExternalLinkSpaceComponent.h"
#include "CSP/Multiplayer/Components/FogSpaceComponent.h"
#include "CSP/Multiplayer/Components/ImageSpaceComponent.h"
#include "CSP/Multiplayer/Components/ReflectionSpaceComponent.h"
#include "CSP/Multiplayer/Components/ScriptSpaceComponent.h"
#include "CSP/Multiplayer/Components/StaticModelSpaceComponent.h"
#include "CSP/Multiplayer/Components/VideoPlayerSpaceComponent.h"
#include "CSP/Multiplayer/MultiPlayerConnection.h"
#include "CSP/Multiplayer/ReplicatedValue.h"
#include "CSP/Multiplayer/Script/EntityScript.h"
#include "CSP/Multiplayer/SpaceEntity.h"
#include "CSP/Multiplayer/SpaceEntitySystem.h"
#include "CSP/Systems/Spaces/Space.h"
#include "CSP/Systems/Spaces/UserRoles.h"
#include "CSP/Systems/SystemsManager.h"
#include "CSP/Systems/Users/UserSystem.h"
#include "Debug/Logging.h"
#include "Memory/Memory.h"
#include "Multiplayer/SpaceEntityKeys.h"
#include "SpaceSystemTestHelpers.h"
#include "TestHelpers.h"
#include "UserSystemTestHelpers.h"
#include "filesystem.hpp"
#include "signalrclient/signalr_value.h"

#include "gtest/gtest.h"
#include <CSP/Multiplayer/Components/ConversationSpaceComponent.h>
#include <CSP/Multiplayer/Components/LightSpaceComponent.h>
#include <CSP/Multiplayer/Components/PortalSpaceComponent.h>
#include <CSP/Multiplayer/Components/SplineSpaceComponent.h>
#include <atomic_queue/defs.h>
#include <chrono>
#include <filesystem>
#include <thread>

using namespace csp::multiplayer;
using namespace std::chrono_literals;

namespace
{

void InitialiseTestingConnection(const csp::systems::Space& Space);
void OnConnect();
void OnDisconnect(bool ok);
void OnUserCreated(SpaceEntity* InUser);
void OnDelete();

std::atomic_bool IsTestComplete;
std::atomic_bool IsDisconnected;
std::atomic_bool IsReadyForUpdate;
MultiplayerConnection* Connection;
SpaceEntitySystem* EntitySystem;
SpaceEntity* TestUser;
SpaceEntity* TestObject;

int WaitForTestTimeoutCountMs;
const int WaitForTestTimeoutLimit	= 20000;
const int NumberOfEntityUpdateTicks = 5;
int ReceivedEntityUpdatesCount;

bool EventSent	   = false;
bool EventReceived = false;

ReplicatedValue ObjectFloatProperty;
ReplicatedValue ObjectBoolProperty;
ReplicatedValue ObjectIntProperty;
ReplicatedValue ObjectStringProperty;

bool RequestPredicate(const csp::services::ResultBase& Result)
{
	return Result.GetResultCode() != csp::services::EResultCode::InProgress;
}

void InitialiseTestingConnection()
{
	IsTestComplete	 = false;
	IsDisconnected	 = false;
	IsReadyForUpdate = false;
	TestUser		 = nullptr;

	WaitForTestTimeoutCountMs  = 0;
	ReceivedEntityUpdatesCount = 0;


	EventSent	  = false;
	EventReceived = false;

	ObjectFloatProperty	 = ReplicatedValue(2.3f);
	ObjectBoolProperty	 = ReplicatedValue(true);
	ObjectIntProperty	 = ReplicatedValue(static_cast<int64_t>(42));
	ObjectStringProperty = "My replicated string";
}


void SetRandomProperties(SpaceEntity* User)
{
	if (User == nullptr)
	{
		return;
	}

	IsReadyForUpdate = false;

	char NameBuffer[10];
	SPRINTF(NameBuffer, "MyName%i", rand() % 100);
	User->SetName(NameBuffer);

	csp::common::Vector3 Position = {static_cast<float>(rand() % 100), static_cast<float>(rand() % 100), static_cast<float>(rand() % 100)};
	User->SetPosition(Position);

	csp::common::Vector4 Rotation
		= {static_cast<float>(rand() % 100), static_cast<float>(rand() % 100), static_cast<float>(rand() % 100), static_cast<float>(rand() % 100)};
	User->SetRotation(Rotation);

	AvatarSpaceComponent* AvatarComponent = static_cast<AvatarSpaceComponent*>(User->GetComponent(0));
	AvatarComponent->SetState(static_cast<AvatarState>(rand() % 6));

	EntitySystem->QueueEntityUpdate(User);
}

void OnConnect()
{
	csp::common::String UserName = "Player 1";
	SpaceTransform UserTransform
		= {csp::common::Vector3 {1.452322f, 2.34f, 3.45f}, csp::common::Vector4 {4.1f, 5.1f, 6.1f, 7.1f}, csp::common::Vector3 {1, 1, 1}};
	csp::common::String UserAvatarId = "MyCoolAvatar";

	AvatarState UserState			  = AvatarState::Idle;
	AvatarPlayMode UserAvatarPlayMode = AvatarPlayMode::Default;

	EntitySystem->CreateAvatar(UserName,
							   UserTransform,
							   UserState,
							   UserAvatarId,
							   UserAvatarPlayMode,
							   [](SpaceEntity* NewAvatar)
							   {
								   EXPECT_NE(NewAvatar, nullptr);

								   std::cerr << "CreateAvatar Local Callback" << std::endl;

								   EXPECT_EQ(NewAvatar->GetEntityType(), SpaceEntityType::Avatar);

								   if (NewAvatar->GetEntityType() == SpaceEntityType::Avatar)
								   {
									   OnUserCreated(NewAvatar);
								   }
							   });
}

void OnDisconnect(bool ok)
{
	EXPECT_TRUE(ok);

	std::cerr << "OnDisconnect" << std::endl;

	IsDisconnected = true;
}

void OnUserCreated(SpaceEntity* InUser)
{
	EXPECT_EQ(InUser->GetComponents()->Size(), 1);

	auto* AvatarComponent = InUser->GetComponent(0);

	EXPECT_EQ(AvatarComponent->GetComponentType(), ComponentType::AvatarData);

	TestUser = InUser;
	TestUser->SetUpdateCallback(
		[InUser](SpaceEntity* UpdatedUser, SpaceEntityUpdateFlags InUpdateFlags, csp::common::Array<ComponentUpdateInfo> InComponentUpdateInfoArray)
		{
			if (InUpdateFlags & SpaceEntityUpdateFlags::UPDATE_FLAGS_NAME)
			{
				std::cerr << "Name Updated: " << UpdatedUser->GetName() << std::endl;
			}

			if (InUpdateFlags & SpaceEntityUpdateFlags::UPDATE_FLAGS_POSITION)
			{
				std::cerr << "Position Updated: X:" << UpdatedUser->GetPosition().X << " Y:" << UpdatedUser->GetPosition().Y
						  << " Z:" << UpdatedUser->GetPosition().Z << std::endl;
			}

			if (InUpdateFlags & SpaceEntityUpdateFlags::UPDATE_FLAGS_ROTATION)
			{
				std::cerr << "Rotation Updated: X:" << UpdatedUser->GetRotation().X << " Y:" << UpdatedUser->GetRotation().Y
						  << " Z:" << UpdatedUser->GetRotation().Z << " W:" << UpdatedUser->GetRotation().W << std::endl;
			}

			if (InUpdateFlags & SpaceEntityUpdateFlags::UPDATE_FLAGS_COMPONENTS)
			{
				for (int i = 0; i < InComponentUpdateInfoArray.Size(); ++i)
				{
					uint16_t ComponentID = InComponentUpdateInfoArray[i].ComponentId;

					if (ComponentID < csp::multiplayer::COMPONENT_KEYS_START_VIEWS)
					{
						std::cerr << "Component Updated: ID: " << ComponentID << std::endl;

						const csp::common::Map<uint32_t, ReplicatedValue>& Properties = *UpdatedUser->GetComponent(ComponentID)->GetProperties();
						const csp::common::Array<uint32_t>* PropertyKeys			  = Properties.Keys();

						for (int j = 0; j < PropertyKeys->Size(); ++j)
						{
							if (j >= 3) // We only randomise the first 3 properties, so we don't really need to print any more
							{
								break;
							}

							uint32_t PropertyID = PropertyKeys->operator[](j);
							std::cerr << "\tProperty ID: " << PropertyID;

							const ReplicatedValue& Property = Properties[PropertyID];

							switch (Property.GetReplicatedValueType())
							{
								case ReplicatedValueType::Integer:
									std::cerr << "\tValue: " << Property.GetInt() << std::endl;
									break;
								case ReplicatedValueType::String:
									std::cerr << "\tValue: " << Property.GetString() << std::endl;
									break;
								case ReplicatedValueType::Float:
									std::cerr << "\tValue: " << Property.GetFloat() << std::endl;
									break;
								case ReplicatedValueType::Boolean:
									std::cerr << "\tValue: " << Property.GetBool() << std::endl;
									break;
								case ReplicatedValueType::Vector3:
									std::cerr << "\tValue: {" << Property.GetVector3().X << ", " << Property.GetVector3().Y << ", "
											  << Property.GetVector3().Z << "}" << std::endl;
									break;
								case ReplicatedValueType::Vector4:
									std::cerr << "\tValue: {" << Property.GetVector4().X << ", " << Property.GetVector4().Y << ", "
											  << Property.GetVector4().Z << ", " << Property.GetVector4().W << "}" << std::endl;
									break;
								default:
									break;
							}
						}

						CSP_DELETE(PropertyKeys);
					}
				}
			}

			if (InUser == TestUser)
			{
				ReceivedEntityUpdatesCount++;
				IsReadyForUpdate = true;
			}
		});

	TestUser->SetDestroyCallback(
		[](bool Ok)
		{
			if (Ok)
			{
				std::cerr << "Destroy Callback Complete!" << std::endl;
			}
		});

	std::cerr << "OnUserCreated" << std::endl;

	SetRandomProperties(InUser);
}

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_MANUAL_SIGNALRCONNECTION_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, ManualConnectionTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();
	auto* AssetSystem	 = SystemsManager.GetAssetSystem();

	const char* TestSpaceName			= "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription	= "OLY-UNITTEST-SPACEDESC-REWIND";
	const char* TestAssetCollectionName = "OLY-UNITTEST-ASSETCOLLECTION-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	char UniqueAssetCollectionName[256];
	SPRINTF(UniqueAssetCollectionName, "%s-%s", TestAssetCollectionName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto* Connection = new csp::multiplayer::MultiplayerConnection(Space.Id);

	auto [Ok] = AWAIT(Connection, Connect);

	EXPECT_TRUE(Ok);

	std::tie(Ok) = AWAIT(Connection, InitialiseConnection);

	EXPECT_TRUE(Ok);

	EntitySystem = Connection->GetSpaceEntitySystem();

	csp::common::String ObjectName = "Object 1";
	SpaceTransform ObjectTransform
		= {csp::common::Vector3 {1.452322f, 2.34f, 3.45f}, csp::common::Vector4 {4.1f, 5.1f, 6.1f, 7.1f}, csp::common::Vector3 {1, 1, 1}};

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	auto [CreatedObject] = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

	EXPECT_EQ(CreatedObject->GetName(), ObjectName);
	EXPECT_EQ(CreatedObject->GetPosition(), ObjectTransform.Position);
	EXPECT_EQ(CreatedObject->GetRotation(), ObjectTransform.Rotation);
	EXPECT_EQ(CreatedObject->GetScale(), ObjectTransform.Scale);

	std::tie(Ok) = AWAIT(Connection, Disconnect);

	EXPECT_TRUE(Ok);

	// Delete MultiplayerConnection
	delete Connection;

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_SIGNALRCONNECTION_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, SignalRConnectionTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();
	auto* AssetSystem	 = SystemsManager.GetAssetSystem();

	const char* TestSpaceName			= "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription	= "OLY-UNITTEST-SPACEDESC-REWIND";
	const char* TestAssetCollectionName = "OLY-UNITTEST-ASSETCOLLECTION-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	char UniqueAssetCollectionName[256];
	SPRINTF(UniqueAssetCollectionName, "%s-%s", TestAssetCollectionName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	InitialiseTestingConnection();

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_NIGHTLY_TESTS
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, SignalRKeepAliveTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();
	auto* AssetSystem	 = SystemsManager.GetAssetSystem();

	const char* TestSpaceName			= "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription	= "OLY-UNITTEST-SPACEDESC-REWIND";
	const char* TestAssetCollectionName = "OLY-UNITTEST-ASSETCOLLECTION-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	char UniqueAssetCollectionName[256];
	SPRINTF(UniqueAssetCollectionName, "%s-%s", TestAssetCollectionName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	InitialiseTestingConnection();

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	WaitForTestTimeoutCountMs = 0;
	int KeepAliveInterval	  = 200000;

	while (WaitForTestTimeoutCountMs < KeepAliveInterval)
	{

		std::this_thread::sleep_for(20ms);
		WaitForTestTimeoutCountMs += 20;
	}

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_ENTITYREPLICATION_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, EntityReplicationTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();
	auto* AssetSystem	 = SystemsManager.GetAssetSystem();

	const char* TestSpaceName			= "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription	= "OLY-UNITTEST-SPACEDESC-REWIND";
	const char* TestAssetCollectionName = "OLY-UNITTEST-ASSETCOLLECTION-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	char UniqueAssetCollectionName[256];
	SPRINTF(UniqueAssetCollectionName, "%s-%s", TestAssetCollectionName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	InitialiseTestingConnection();

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();
	OnConnect();
	WaitForTestTimeoutCountMs = 0;

	while (!IsTestComplete && WaitForTestTimeoutCountMs < WaitForTestTimeoutLimit)
	{
		EntitySystem->ProcessPendingEntityOperations();

		std::this_thread::sleep_for(50ms);
		WaitForTestTimeoutCountMs += 50;

		if (ReceivedEntityUpdatesCount < NumberOfEntityUpdateTicks)
		{
			if (IsReadyForUpdate)
			{
				SetRandomProperties(TestUser);
			}
		}
		else if (ReceivedEntityUpdatesCount == NumberOfEntityUpdateTicks && IsReadyForUpdate) // Send a final update that doesn't change the data
		{
			IsReadyForUpdate = false;
			EntitySystem->QueueEntityUpdate(TestUser);
		}
		else
		{
			IsTestComplete = true;
		}
	}

	EXPECT_TRUE(IsTestComplete);

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_SELF_REPLICATION_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, SelfReplicationTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();
	auto* AssetSystem	 = SystemsManager.GetAssetSystem();

	const char* TestSpaceName			= "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription	= "OLY-UNITTEST-SPACEDESC-REWIND";
	const char* TestAssetCollectionName = "OLY-UNITTEST-ASSETCOLLECTION-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	char UniqueAssetCollectionName[256];
	SPRINTF(UniqueAssetCollectionName, "%s-%s", TestAssetCollectionName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);

	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);

	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	auto [FlagSetResult] = AWAIT(Connection, SetAllowSelfMessagingFlag, true);

	if (FlagSetResult)
	{
		csp::common::String ObjectName = "Object 1";
		SpaceTransform ObjectTransform
			= {csp::common::Vector3 {1.452322f, 2.34f, 3.45f}, csp::common::Vector4 {4.1f, 5.1f, 6.1f, 7.1f}, csp::common::Vector3 {1, 1, 1}};

		EntitySystem->SetEntityCreatedCallback(
			[](SpaceEntity* Entity)
			{
			});

		auto [CreatedObject] = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

		EXPECT_EQ(CreatedObject->GetName(), ObjectName);
		EXPECT_EQ(CreatedObject->GetPosition(), ObjectTransform.Position);
		EXPECT_EQ(CreatedObject->GetRotation(), ObjectTransform.Rotation);
		EXPECT_EQ(CreatedObject->GetScale(), ObjectTransform.Scale);

		auto ModelComponent = dynamic_cast<StaticModelSpaceComponent*>(CreatedObject->AddComponent(ComponentType::StaticModel));
		ModelComponent->SetModelAssetId("SomethingElse");
		ModelComponent->SetAssetCollectionId("Something");

		bool EntityUpdated = false;

		CreatedObject->SetUpdateCallback(
			[&EntityUpdated](SpaceEntity* Entity, SpaceEntityUpdateFlags Flags, csp::common::Array<ComponentUpdateInfo>& UpdateInfo)
			{
				if (Entity->GetName() == "Object 1")
				{
					if (Flags & SpaceEntityUpdateFlags::UPDATE_FLAGS_SCALE)
					{
						std::cerr << "Scale Updated" << std::endl;
						EntityUpdated = true;
					}
				}
			});
		CreatedObject->SetScale(csp::common::Vector3 {3.0f, 3.0f, 3.0f});
		CreatedObject->QueueUpdate();

		while (!EntityUpdated && WaitForTestTimeoutCountMs < WaitForTestTimeoutLimit)
		{
			EntitySystem->ProcessPendingEntityOperations();
			std::this_thread::sleep_for(50ms);
			WaitForTestTimeoutCountMs += 50;
		}

		EXPECT_LE(WaitForTestTimeoutCountMs, WaitForTestTimeoutLimit);

		EXPECT_EQ(CreatedObject->GetScale().X, 3.0f);
		EXPECT_EQ(CreatedObject->GetScale().Y, 3.0f);
		EXPECT_EQ(CreatedObject->GetScale().Z, 3.0f);
	}

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_CREATE_AVATAR_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, CreateAvatarTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	const csp::common::String& UserName = "Player 1";
	const SpaceTransform& UserTransform
		= {csp::common::Vector3 {1.452322f, 2.34f, 3.45f}, csp::common::Vector4 {4.1f, 5.1f, 6.1f, 7.1f}, csp::common::Vector3 {1, 1, 1}};
	AvatarState UserAvatarState				  = AvatarState::Idle;
	const csp::common::String& UserAvatarId	  = "MyCoolAvatar";
	AvatarPlayMode UserAvatarPlayMode		  = AvatarPlayMode::Default;
	LocomotionModel UserAvatarLocomotionModel = LocomotionModel::Grounded;

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	auto [Avatar] = AWAIT(EntitySystem, CreateAvatar, UserName, UserTransform, UserAvatarState, UserAvatarId, UserAvatarPlayMode);
	EXPECT_NE(Avatar, nullptr);

	EXPECT_EQ(Avatar->GetEntityType(), SpaceEntityType::Avatar);
	EXPECT_EQ(Avatar->GetName(), UserName);
	EXPECT_EQ(Avatar->GetPosition(), UserTransform.Position);
	EXPECT_EQ(Avatar->GetRotation(), UserTransform.Rotation);

	auto& Components = *Avatar->GetComponents();

	EXPECT_EQ(Components.Size(), 1);

	auto* Component = Components[0];

	EXPECT_EQ(Component->GetComponentType(), ComponentType::AvatarData);

	// Verify the values of UserAvatarState and UserAvatarPlayMode
	AvatarSpaceComponent* AvatarComponent = dynamic_cast<AvatarSpaceComponent*>(Component);
	EXPECT_NE(AvatarComponent, nullptr);
	EXPECT_EQ(AvatarComponent->GetState(), UserAvatarState);
	EXPECT_EQ(AvatarComponent->GetAvatarPlayMode(), UserAvatarPlayMode);
	EXPECT_EQ(AvatarComponent->GetLocomotionModel(), UserAvatarLocomotionModel);

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif


#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_CREATE_CREATOR_AVATAR_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, CreateCreatorAvatarTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	const csp::common::String& UserName = "Creator 1";
	const SpaceTransform& UserTransform
		= {csp::common::Vector3 {1.452322f, 2.34f, 3.45f}, csp::common::Vector4 {4.1f, 5.1f, 6.1f, 7.1f}, csp::common::Vector3 {1, 1, 1}};
	AvatarState UserAvatarState				  = AvatarState::Idle;
	const csp::common::String& UserAvatarId	  = "MyCoolCreatorAvatar";
	AvatarPlayMode UserAvatarPlayMode		  = AvatarPlayMode::Creator;
	LocomotionModel UserAvatarLocomotionModel = LocomotionModel::Grounded;

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	auto [Avatar] = AWAIT(EntitySystem, CreateAvatar, UserName, UserTransform, UserAvatarState, UserAvatarId, UserAvatarPlayMode);
	EXPECT_NE(Avatar, nullptr);

	EXPECT_EQ(Avatar->GetEntityType(), SpaceEntityType::Avatar);
	EXPECT_EQ(Avatar->GetName(), UserName);
	EXPECT_EQ(Avatar->GetPosition(), UserTransform.Position);
	EXPECT_EQ(Avatar->GetRotation(), UserTransform.Rotation);

	auto& Components = *Avatar->GetComponents();

	EXPECT_EQ(Components.Size(), 1);

	auto* Component = Components[0];

	EXPECT_EQ(Component->GetComponentType(), ComponentType::AvatarData);

	// Verify the values of UserAvatarState and UserAvatarPlayMode
	AvatarSpaceComponent* AvatarComponent = dynamic_cast<AvatarSpaceComponent*>(Component);
	EXPECT_NE(AvatarComponent, nullptr);
	EXPECT_EQ(AvatarComponent->GetState(), UserAvatarState);
	EXPECT_EQ(AvatarComponent->GetAvatarPlayMode(), UserAvatarPlayMode);
	EXPECT_EQ(AvatarComponent->GetAvatarPlayMode(), AvatarPlayMode::Creator);
	EXPECT_EQ(AvatarComponent->GetLocomotionModel(), UserAvatarLocomotionModel);


	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif


#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_AVATAR_MOVEMENT_DIRECTION_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, AvatarMovementDirectionTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	const csp::common::String& UserName = "Player 1";
	const SpaceTransform& UserTransform
		= {csp::common::Vector3 {1.452322f, 2.34f, 3.45f}, csp::common::Vector4 {4.1f, 5.1f, 6.1f, 7.1f}, csp::common::Vector3 {1, 1, 1}};
	AvatarState UserAvatarState				= AvatarState::Idle;
	const csp::common::String& UserAvatarId = "MyCoolAvatar";
	AvatarPlayMode UserAvatarPlayMode		= AvatarPlayMode::Default;

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	auto [Avatar] = AWAIT(EntitySystem, CreateAvatar, UserName, UserTransform, UserAvatarState, UserAvatarId, UserAvatarPlayMode);
	EXPECT_NE(Avatar, nullptr);

	auto& Components = *Avatar->GetComponents();
	EXPECT_EQ(Components.Size(), 1);

	auto* Component = Components[0];
	EXPECT_EQ(Component->GetComponentType(), ComponentType::AvatarData);

	AvatarSpaceComponent* AvatarComponent = dynamic_cast<AvatarSpaceComponent*>(Component);
	EXPECT_NE(AvatarComponent, nullptr);

	// test setting and getting movement direction
	AvatarComponent->SetMovementDirection(csp::common::Vector3::One());

	Avatar->QueueUpdate();

	EXPECT_EQ(AvatarComponent->GetMovementDirection(), csp::common::Vector3::One());

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif


#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_OBJECT_CREATE_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, ObjectCreateTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();
	auto* AssetSystem	 = SystemsManager.GetAssetSystem();

	const char* TestSpaceName			= "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription	= "OLY-UNITTEST-SPACEDESC-REWIND";
	const char* TestAssetCollectionName = "OLY-UNITTEST-ASSETCOLLECTION-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	char UniqueAssetCollectionName[256];
	SPRINTF(UniqueAssetCollectionName, "%s-%s", TestAssetCollectionName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	InitialiseTestingConnection();

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	csp::common::String ObjectName = "Object 1";
	SpaceTransform ObjectTransform
		= {csp::common::Vector3 {1.452322f, 2.34f, 3.45f}, csp::common::Vector4 {4.1f, 5.1f, 6.1f, 7.1f}, csp::common::Vector3 {1, 1, 1}};

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	auto [CreatedObject] = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

	EXPECT_EQ(CreatedObject->GetName(), ObjectName);
	EXPECT_EQ(CreatedObject->GetPosition(), ObjectTransform.Position);
	EXPECT_EQ(CreatedObject->GetRotation(), ObjectTransform.Rotation);
	EXPECT_EQ(CreatedObject->GetScale(), ObjectTransform.Scale);
	EXPECT_EQ(CreatedObject->GetThirdPartyRef(), "");
	EXPECT_EQ(CreatedObject->GetThirdPartyPlatformType(), csp::systems::EThirdPartyPlatform::NONE);


	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_OBJECT_ADDCOMPONENT_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, ObjectAddComponentTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	const csp::common::String ObjectName = "Object 1";
	SpaceTransform ObjectTransform		 = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};

	auto [Object] = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

	bool PatchPending = true;
	Object->SetPatchSentCallback(
		[&PatchPending](bool ok)
		{
			PatchPending = false;
		});

	const csp::common::String ModelAssetId = "NotARealId";

	auto* StaticModelComponent	 = (StaticModelSpaceComponent*) Object->AddComponent(ComponentType::StaticModel);
	auto StaticModelComponentKey = StaticModelComponent->GetId();
	StaticModelComponent->SetModelAssetId(ModelAssetId);
	Object->QueueUpdate();

	while (PatchPending)
	{
		EntitySystem->ProcessPendingEntityOperations();
		std::this_thread::sleep_for(10ms);
	}

	PatchPending = true;

	auto& Components = *Object->GetComponents();

	EXPECT_EQ(Components.Size(), 1);
	EXPECT_TRUE(Components.HasKey(StaticModelComponentKey));

	auto* _StaticModelComponent = Object->GetComponent(StaticModelComponentKey);

	EXPECT_EQ(_StaticModelComponent->GetComponentType(), ComponentType::StaticModel);
	auto* RealStaticModelComponent = (StaticModelSpaceComponent*) _StaticModelComponent;

	EXPECT_EQ(RealStaticModelComponent->GetModelAssetId(), ModelAssetId);

	const csp::common::String ImageAssetId = "AlsoNotARealId";

	auto* ImageComponent		= (ImageSpaceComponent*) Object->AddComponent(ComponentType::Image);
	auto ImageModelComponentKey = ImageComponent->GetId();
	ImageComponent->SetImageAssetId(ImageAssetId);
	Object->QueueUpdate();

	while (PatchPending)
	{
		EntitySystem->ProcessPendingEntityOperations();
		std::this_thread::sleep_for(10ms);
	}

	EXPECT_EQ(Object->GetComponents()->Size(), 2);
	EXPECT_TRUE(Components.HasKey(StaticModelComponentKey));
	EXPECT_TRUE(Components.HasKey(ImageModelComponentKey));

	auto* _ImageComponent = Object->GetComponent(ImageModelComponentKey);

	EXPECT_EQ(_ImageComponent->GetComponentType(), ComponentType::Image);
	auto* RealImageComponent = (ImageSpaceComponent*) _ImageComponent;

	EXPECT_EQ(RealImageComponent->GetImageAssetId(), ImageAssetId);

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_OBJECT_REMOVECOMPONENT_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, ObjectRemoveComponentTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	const csp::common::String ObjectName = "Object 1";
	SpaceTransform ObjectTransform		 = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};

	auto [Object] = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

	bool PatchPending = true;
	Object->SetPatchSentCallback(
		[&PatchPending](bool ok)
		{
			PatchPending = false;
		});

	const csp::common::String ModelAssetId = "NotARealId";

	auto* StaticModelComponent	 = (StaticModelSpaceComponent*) Object->AddComponent(ComponentType::StaticModel);
	auto StaticModelComponentKey = StaticModelComponent->GetId();
	StaticModelComponent->SetModelAssetId(ModelAssetId);
	auto* ImageComponent   = (ImageSpaceComponent*) Object->AddComponent(ComponentType::Image);
	auto ImageComponentKey = ImageComponent->GetId();
	ImageComponent->SetImageAssetId("TestID");
	Object->QueueUpdate();

	while (PatchPending)
	{
		EntitySystem->ProcessPendingEntityOperations();
		std::this_thread::sleep_for(10ms);
	}

	PatchPending = true;

	auto& Components = *Object->GetComponents();

	EXPECT_EQ(Components.Size(), 2);
	EXPECT_TRUE(Components.HasKey(StaticModelComponentKey));
	EXPECT_TRUE(Components.HasKey(ImageComponentKey));

	auto* _StaticModelComponent = Object->GetComponent(StaticModelComponentKey);

	EXPECT_EQ(_StaticModelComponent->GetComponentType(), ComponentType::StaticModel);
	auto* RealStaticModelComponent = (StaticModelSpaceComponent*) _StaticModelComponent;

	EXPECT_EQ(RealStaticModelComponent->GetModelAssetId(), ModelAssetId);

	Object->RemoveComponent(StaticModelComponentKey);
	Object->RemoveComponent(ImageComponentKey);

	Object->QueueUpdate();

	while (PatchPending)
	{
		EntitySystem->ProcessPendingEntityOperations();
		std::this_thread::sleep_for(10ms);
	}

	auto& RealComponents = *Object->GetComponents();

	EXPECT_EQ(RealComponents.Size(), 0);
	EXPECT_FALSE(RealComponents.HasKey(StaticModelComponentKey));
	EXPECT_FALSE(RealComponents.HasKey(ImageComponentKey));

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_CREATE_SCRIPT_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, CreateScriptTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	// we'll be using this in a few places below as part of the test, so we declare it upfront
	const std::string ScriptText = R"xx(

         var entities = TheEntitySystem.getEntities();
		  var entityIndex = TheEntitySystem.getIndexOfEntity(ThisEntity.id);

		  globalThis.onClick = (_evtName, params) => {
		    const { id, cid } = JSON.parse(params);
		    CSP.Log(`Clicked entityId: ${id} componentId: ${cid}`);
		  }

		  globalThis.onTick = () => {
		    CSP.Log('Tick');
		  }

		  ThisEntity.subscribeToMessage("buttonPressed", "onClick");
		  ThisEntity.subscribeToMessage("entityTick", "onTick");

			CSP.Log('Printing to the log from a script');
		  
    )xx";

	// Let's create a simple script and see if we can invoke it OK
	{
		EntitySystem->SetEntityCreatedCallback(
			[](SpaceEntity* Entity)
			{
			});

		const csp::common::String ObjectName  = "Object 1";
		SpaceTransform ObjectTransform		  = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};
		auto [Object]						  = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);
		ScriptSpaceComponent* ScriptComponent = static_cast<ScriptSpaceComponent*>(Object->AddComponent(ComponentType::ScriptData));

		ScriptComponent->SetScriptSource(csp::common::String(ScriptText.c_str()));
		Object->GetScript()->Invoke();

		const bool ScriptHasErrors = Object->GetScript()->HasError();
		EXPECT_FALSE(ScriptHasErrors);

		Object->QueueUpdate();

		EntitySystem->ProcessPendingEntityOperations();
	}

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_RUN_SCRIPT_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, RunScriptTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	std::atomic_bool ScriptSystemReady = false;

	auto EntityCreatedCallback = [](SpaceEntity* Entity)
	{
		std::cerr << "EntityCreatedCallback called" << std::endl;
	};

	auto EntitiesReadyCallback = [](bool Ok)
	{
		EXPECT_EQ(Ok, true);
		std::cerr << "EntitiesReadyCallback called" << std::endl;
	};

	auto ScriptSystemReadyCallback = [&ScriptSystemReady](bool Ok)
	{
		EXPECT_EQ(Ok, true);
		std::cerr << "ScriptSystemReadyCallback called" << std::endl;
		ScriptSystemReady = true;
	};

	SpaceSystem->SetEntityCreatedCallback(EntityCreatedCallback);
	SpaceSystem->SetInitialEntitiesRetrievedCallback(EntitiesReadyCallback);
	SpaceSystem->SetScriptSystemReadyCallback(ScriptSystemReadyCallback);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();
	OnConnect();

	// we'll be using this in a few places below as part of the test, so we declare it upfront
	const std::string ScriptText = R"xx(

        var entities = TheEntitySystem.getEntities();
		var entityIndex = TheEntitySystem.getIndexOfEntity(ThisEntity.id);
		
		globalThis.onTick = () => {
            OKO.Log('onTick Called');
			var model = entities[entityIndex].getAnimatedModelComponents()[0];
			model.position = [10, 10, 10];
		}

		ThisEntity.subscribeToMessage("entityTick", "onTick");
		  
    )xx";

	auto ScriptSystemIsReady = [&ScriptSystemReady]()
	{
		std::cerr << "Waiting for ScriptSystemReady" << std::endl;
		return (ScriptSystemReady == true);
	};

	EXPECT_EQ(ResponseWaiter::WaitFor(ScriptSystemIsReady, std::chrono::seconds(5)), true);

	// Create an AnimatedModelComponent and have the script update it's position
	{
		EntitySystem->SetEntityCreatedCallback(
			[](SpaceEntity* Entity)
			{
			});

		const csp::common::String ObjectName = "Object 1";
		SpaceTransform ObjectTransform		 = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};
		auto [Object]						 = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

		AnimatedModelSpaceComponent* AnimatedModelComponent
			= static_cast<AnimatedModelSpaceComponent*>(Object->AddComponent(ComponentType::AnimatedModel));
		ScriptSpaceComponent* ScriptComponent = static_cast<ScriptSpaceComponent*>(Object->AddComponent(ComponentType::ScriptData));

		Object->QueueUpdate();
		EntitySystem->ProcessPendingEntityOperations();

		ScriptComponent->SetScriptSource(csp::common::String(ScriptText.c_str()));
		Object->GetScript()->Invoke();

		csp::CSPFoundation::Tick();

		const bool ScriptHasErrors = Object->GetScript()->HasError();
		EXPECT_FALSE(ScriptHasErrors);

		EXPECT_EQ(AnimatedModelComponent->GetPosition().X, 10.f);
		EXPECT_EQ(AnimatedModelComponent->GetPosition().Y, 10.f);
		EXPECT_EQ(AnimatedModelComponent->GetPosition().Z, 10.f);
	}

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}

#endif

// Removing until we have a proper fix for https://magnopus.atlassian.net/browse/OB-329
/*
#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_MODIFY_EXISTING_SCRIPT_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, ModifyExistingScriptTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto* Connection   = new csp::multiplayer::MultiplayerConnection(Space.Id);
	auto* EntitySystem = Connection->GetSpaceEntitySystem();

	// Connect to the SignalR server
	auto [Ok] = Awaitable(&MultiplayerConnection::Connect, Connection).Await();
	if(!Ok) { return; }

	std::tie(Ok) = Awaitable(&MultiplayerConnection::InitialiseConnection, Connection).Await();
	if(!Ok) { return; }

	// we'll be using this in a few places below as part of the test, so we declare it upfront
	const std::string ScriptText = R"xx(

		 var entities = TheEntitySystem.getEntities();
		  var entityIndex = TheEntitySystem.getIndexOfEntity(ThisEntity.id);

		  globalThis.onClick = (_evtName, params) => {
			const { id, cid } = JSON.parse(params);
			CSP.Log(`Clicked entityId: ${id} componentId: ${cid}`);
		  }

		  globalThis.onTick = () => {
			CSP.Log('Tick');
		  }

		  ThisEntity.subscribeToMessage("buttonPressed", "onClick");
		  ThisEntity.subscribeToMessage("entityTick", "onTick");

			CSP.Log('Printing to the log from a script');

	)xx";

	// For our first phase of this script test, we simply an object with a script component, and assign it
	// a valid script, tell CHS about it and then bail out of the connection
	{
		const csp::common::String ObjectName = "Object 1";
		SpaceTransform ObjectTransform		= {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};
		auto [Object] = Awaitable(&SpaceEntitySystem::CreateObject, EntitySystem, ObjectName, ObjectTransform).Await();
		ScriptSpaceComponent* ScriptComponent	 = static_cast<ScriptSpaceComponent*>(Object->AddComponent(ComponentType::ScriptData));

		ScriptComponent->SetScriptSource(csp::common::String(ScriptText.c_str()));
		Object->QueueUpdate();

		EntitySystem->ProcessPendingEntityOperations();
	}

	// Disconnect from the SignalR server
	std::tie(Ok) = Awaitable(&MultiplayerConnection::Disconnect, Connection).Await();
	if(!Ok) { return; }

	// Delete MultiplayerConnection
	delete Connection;

	//------------------------------------------------------------
	// For our second phase of the test, we attempt to take an entity that already exists (we created it in phase 1), modify the script source and
	// re-invoke the script

	// Re-form the connection for the second phase of this test
	Connection   = new csp::multiplayer::MultiplayerConnection(Space.Id);
	EntitySystem = Connection->GetSpaceEntitySystem();

	bool EntityHasBeenRecreated = false;
	// we're gonna wanna wait till the entity is created before we can do our test
	EntitySystem->SetEntityCreatedCallback(
	[&EntityHasBeenRecreated](csp::multiplayer::SpaceEntity* Object)
	{
		EntityHasBeenRecreated = true;
	});

	// Connect to the SignalR server
	auto [Ok2] = Awaitable(&MultiplayerConnection::Connect, Connection).Await();
	if(!Ok2) { return; }

	std::tie(Ok2) = Awaitable(&MultiplayerConnection::InitialiseConnection, Connection).Await();
	if(!Ok2) { return; }

	// spin till we recreate the entity from phase 1 locally, having received it back from CHS
	while(EntityHasBeenRecreated == false) {}

	// interesting part of phase 2 begins!
	{
		csp::multiplayer::SpaceEntity* Object = EntitySystem->GetEntityByIndex(0);
		// grab the script component we created in phase 1 (we should make this kind of thing easier)
		const csp::common::Array<ComponentBase*>& Components = *Object->GetComponents()->Values();
		ScriptSpaceComponent* ScriptComponent = nullptr;
		for(size_t i = 0; Components.Size(); i++)
		{
			if(Components[i]->GetComponentType() == csp::multiplayer::ComponentType::ScriptData)
			{
				ScriptComponent = dynamic_cast<ScriptSpaceComponent*>(Components[i]);
				break;
			}
		}

		// phew! now we have that we can attempt to modify script source again and re-invoke - this is the part that we really want to test
		// can we successfully modify a pre-existing script, and re-invoke it without script errors?
		ScriptComponent->SetScriptSource(csp::common::String(ScriptText.c_str()));
		Object->GetScript()->Invoke();

		const bool ScriptHasErrors = Object->GetScript()->HasError();
		EXPECT_FALSE(ScriptHasErrors);
	}

	// Disconnect from the SignalR server
	std::tie(Ok2) = Awaitable(&MultiplayerConnection::Disconnect, Connection).Await();
	if(!Ok2) { return; }

	// Delete MultiplayerConnection
	delete Connection;

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif
*/

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_NETWORKEVENT_EMPTY_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, NetworkEventEmptyTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();
	auto* AssetSystem	 = SystemsManager.GetAssetSystem();

	const char* TestSpaceName			= "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription	= "OLY-UNITTEST-SPACEDESC-REWIND";
	const char* TestAssetCollectionName = "OLY-UNITTEST-ASSETCOLLECTION-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	char UniqueAssetCollectionName[256];
	SPRINTF(UniqueAssetCollectionName, "%s-%s", TestAssetCollectionName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	Connection->ListenNetworkEvent("TestEvent",
								   [](bool ok, csp::common::Array<ReplicatedValue> Data)
								   {
									   EXPECT_TRUE(ok);

									   std::cerr << "Test Event Received " << ok << std::endl;
								   });

	Connection->ListenNetworkEvent("TestEvent",
								   [](bool ok, csp::common::Array<ReplicatedValue> Data)
								   {
									   EXPECT_TRUE(ok);

									   EventReceived = true;

									   if (EventSent)
									   {
										   IsTestComplete = true;
									   }

									   std::cerr << "Second Test Event Received " << ok << std::endl;
								   });

	Connection->SendNetworkEventToClient("TestEvent",
										 {},
										 Connection->GetClientId(),
										 [](bool ok)
										 {
											 EXPECT_TRUE(ok);

											 EventSent = true;

											 if (EventReceived)
											 {
												 IsTestComplete = true;
											 }

											 std::cerr << "Test Event Sent " << ok << std::endl;
										 });

	while (!IsTestComplete && WaitForTestTimeoutCountMs < WaitForTestTimeoutLimit)
	{
		std::this_thread::sleep_for(50ms);
		WaitForTestTimeoutCountMs += 50;
	}

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_NETWORKEVENT_MULTITYPE_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, NetworkEventMultiTypeTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();
	auto* AssetSystem	 = SystemsManager.GetAssetSystem();

	const char* TestSpaceName			= "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription	= "OLY-UNITTEST-SPACEDESC-REWIND";
	const char* TestAssetCollectionName = "OLY-UNITTEST-ASSETCOLLECTION-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	char UniqueAssetCollectionName[256];
	SPRINTF(UniqueAssetCollectionName, "%s-%s", TestAssetCollectionName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	InitialiseTestingConnection();

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	Connection->ListenNetworkEvent("MultiTypeEvent",
								   [](bool ok, csp::common::Array<ReplicatedValue> Data)
								   {
									   EXPECT_TRUE(ok);

									   std::cerr << "Multi Type Event Received " << ok << "  Payload: " << std::endl;

									   for (int i = 0; i < Data.Size(); ++i)
									   {
										   if (Data[i].GetReplicatedValueType() == ReplicatedValueType::Boolean)
										   {
											   printf("%s\n", Data[i].GetBool() ? "true" : "false");
										   }
										   else if (Data[i].GetReplicatedValueType() == ReplicatedValueType::Integer)
										   {
											   printf("%lli\n", Data[i].GetInt());
										   }
										   else if (Data[i].GetReplicatedValueType() == ReplicatedValueType::Float)
										   {
											   printf("%f\n", Data[i].GetFloat());
										   }
									   }

									   EventReceived = true;

									   if (EventSent)
									   {
										   IsTestComplete = true;
									   }
								   });

	ReplicatedValue EventInt((int64_t) -1);
	ReplicatedValue EventFloat(1234.567890f);

	Connection->SendNetworkEventToClient("MultiTypeEvent",
										 {EventInt, EventFloat},
										 Connection->GetClientId(),
										 [EventInt, EventFloat](bool ok)
										 {
											 EXPECT_TRUE(ok);

											 EventSent = true;

											 if (EventReceived)
											 {
												 IsTestComplete = true;
											 }

											 printf("%lli, %f, \n", EventInt.GetInt(), EventFloat.GetFloat());
										 });

	while (!IsTestComplete && WaitForTestTimeoutCountMs < WaitForTestTimeoutLimit)
	{
		std::this_thread::sleep_for(50ms);
		WaitForTestTimeoutCountMs += 50;
	}

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_MULTIPLAYER_INTERACTIVEMOVEMENT_TEST
	#include <conio.h>

csp::multiplayer::MultiplayerConnection* Connection;

void OnEntityUpdate(Entity* Object, EntityUpdateType UpdateType)
{
	if (UpdateType == EntityUpdateType::Delete)
	{
		std::cerr << "Got ObjectDelete: " << Object->GetId() << std::endl;

		return;
	}

	const auto& Trans = Object->GetTransform();
	auto Pos		  = Trans.Position;
	auto RepVals	  = Object->GetReplicatedValues();

	std::cerr << "Got ObjectUpdate: " << Object->GetId() << "(" << Pos.X << ", " << Pos.Y << ", " << Pos.Z << ") [";

	for (int i = 0; i < RepVals.Size(); ++i)
	{
		switch (RepVals[i].GetReplicatedValueType())
		{
			case csp::multiplayer::ReplicatedValueType::Boolean:
				std::cerr << RepVals[i].GetBool() << ", ";
				break;
			case csp::multiplayer::ReplicatedValueType::Integer:
				std::cerr << RepVals[i].GetInt() << ", ";
				break;
			case csp::multiplayer::ReplicatedValueType::Float:
				std::cerr << RepVals[i].GetFloat() << ", ";
				break;
			case csp::multiplayer::ReplicatedValueType::String:
				std::cerr << RepVals[i].GetString() << ", ";
				break;
		}
	}

	std::cerr << "]" << std::endl;
}

CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, InteractiveMovementTest)
{
	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();

	std::cout << "Email: ";
	std::string Email;
	std::cin >> Email;
	std::cout << "Password: ";
	std::string Password;
	std::cin >> Password;

	auto [LoginState] = Awaitable(&csp::systems::UserSystem::Login, UserSystem, CSP_TEXT(""), CSP_TEXT(Email.c_str()), CSP_TEXT(Password.c_str()))
							.Await(
								[](const auto& s)
								{
									return s.GetResultCode() != csp::services::EResultCode::InProgress;
								});

	std::cout << "Space Id: ";
	std::string SpaceId;
	std::cin >> SpaceId;

	Connection = new MultiplayerConnection(SpaceId.c_str());

	Connection->RegisterEntityCreatedCallback(
		[](Entity* Object)
		{
			OnEntityUpdate(Object, EntityUpdateType::Update);
			Object->SetRemoteUpdateCallback(OnEntityUpdate);
		});

	auto [Ok] = AWAIT(Connection, Connect);

	EXPECT_TRUE(Ok);

	if (!Ok)
	{
		return;
	}

	time_t _time;
	srand((unsigned int) time(&_time));
	uint64_t PrefabId	= rand() % 1000;
	uint64_t ParentId	= 0;
	bool InIsPersistent = false;
	EntityTransform ObjectTransform;
	ObjectTransform.Position = {0, 0, 0};
	ObjectTransform.Rotation = {0, 0, 0, 0};
	ObjectTransform.Scale	 = {1, 1, 1};
	csp::common::Array<ReplicatedValue> ReplicatedValues;

	auto [Object] = Awaitable(&MultiplayerConnection::CreateEntity,
							  Connection,
							  PrefabId,
							  ParentId,
							  InIsPersistent,
							  ObjectTransform,
							  csp::multiplayer::EntityType::Custom,
							  1ULL,
							  ReplicatedValues)
						.Await();

	EXPECT_TRUE(Object != nullptr);

	if (Object == nullptr)
	{
		return;
	}

	std::cerr << "Object created: " << Object->GetId() << std::endl;
	Object->SetIsLocal(true);

	std::tie(Ok) = Awaitable(&MultiplayerConnection::SendEntity, Connection, Object).Await();

	EXPECT_TRUE(Ok);

	if (!Ok)
	{
		return;
	}

	std::cerr << "Object sent: " << Object->GetId() << std::endl;

	for (;;)
	{
		auto c = _getch();

		if (c == 0x1B) // Escape
		{
			break;
		}

		if (c == 0 || c == 0xE0) // Function/Arrow
		{
			c = _getch();

			switch (c)
			{
				case 0x48: // Up Arrow
				{
					EntityTransform Transform(Object->GetTransform());
					++Transform.Position.Z;
					Object->SetTransform(Transform);
				}
				break;
				case 0x4B: // Left Arrow
				{
					EntityTransform Transform(Object->GetTransform());
					--Transform.Position.X;
					Object->SetTransform(Transform);
				}
				break;
				case 0x4D: // Right Arrow
				{
					EntityTransform Transform(Object->GetTransform());
					++Transform.Position.X;
					Object->SetTransform(Transform);
				}
				break;
				case 0x50: // Down Arrow
				{
					EntityTransform Transform(Object->GetTransform());
					--Transform.Position.Z;
					Object->SetTransform(Transform);
				}
				break;
			}

			Awaitable(&MultiplayerConnection::UpdateEntity, Connection, Object).Await();
		}
	}

	std::tie(Ok) = AWAIT(Connection, Disconnect);

	EXPECT_TRUE(Ok);

	if (!Ok)
	{
		return;
	}

	std::cerr << "Disconnected" << std::endl;

	LogOut(*UserSystem);
}
#endif


#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_AVATAR_SCRIPT_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, AvatarScriptTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	csp::common::String UserName = "Player 1";
	SpaceTransform UserTransform
		= {csp::common::Vector3 {1.452322f, 2.34f, 3.45f}, csp::common::Vector4 {4.1f, 5.1f, 6.1f, 7.1f}, csp::common::Vector3 {1, 1, 1}};
	AvatarState UserAvatarState		  = AvatarState::Idle;
	csp::common::String UserAvatarId  = "MyCoolAvatar";
	AvatarPlayMode UserAvatarPlayMode = AvatarPlayMode::Default;

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	auto [Avatar] = AWAIT(EntitySystem, CreateAvatar, UserName, UserTransform, UserAvatarState, UserAvatarId, UserAvatarPlayMode);

	EXPECT_EQ(Avatar->GetEntityType(), SpaceEntityType::Avatar);
	EXPECT_EQ(Avatar->GetName(), UserName);
	// TODO: Verify these values
	/*EXPECT_EQ(Avatar->GetPosition(), UserTransform.Position);
	EXPECT_EQ(Avatar->GetRotation(), UserTransform.Rotation);*/

	std::string AvatarScriptText = R"xx(

        import * as CSP from "CSP";

        CSP.Log("Entering AvatarScriptTest Script");

        var avatars = TheEntitySystem.getAvatars();

        for (let i=0; i<avatars.length; ++i)
        {
            CSP.Log(JSON.stringify(avatars[i].name));
            CSP.Log(JSON.stringify(avatars[i].id));
            CSP.Log(JSON.stringify(avatars[i].position));
            CSP.Log(JSON.stringify(avatars[i].rotation));
            CSP.Log(JSON.stringify(avatars[i].scale));
        }

        avatars[0].position = [3, 2, 5];
        CSP.Log(JSON.stringify(avatars[0].position));

    )xx";

	Avatar->GetScript()->SetScriptSource(AvatarScriptText.c_str());
	Avatar->GetScript()->Invoke();

	EntitySystem->ProcessPendingEntityOperations();

	auto& Components = *Avatar->GetComponents();

	EXPECT_EQ(Components.Size(), 2);

	auto* Component = Components[0];

	EXPECT_EQ(Component->GetComponentType(), ComponentType::AvatarData);

	auto* ScriptComponent = Components[1];

	EXPECT_EQ(ScriptComponent->GetComponentType(), ComponentType::ScriptData);

	// Delete MultiplayerConnection
	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_SCRIPT_LOG_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, ScriptLogTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	csp::common::String UserName = "Player 1";
	SpaceTransform UserTransform
		= {csp::common::Vector3 {1.452322f, 2.34f, 3.45f}, csp::common::Vector4 {4.1f, 5.1f, 6.1f, 7.1f}, csp::common::Vector3 {1, 1, 1}};
	AvatarState UserAvatarState		  = AvatarState::Idle;
	csp::common::String UserAvatarId  = "MyCoolAvatar";
	AvatarPlayMode UserAvatarPlayMode = AvatarPlayMode::Default;

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	auto [Avatar] = AWAIT(EntitySystem, CreateAvatar, UserName, UserTransform, UserAvatarState, UserAvatarId, UserAvatarPlayMode);

	EXPECT_EQ(Avatar->GetEntityType(), SpaceEntityType::Avatar);
	EXPECT_EQ(Avatar->GetName(), UserName);

	std::string AvatarScriptText = R"xx(

        import * as CSP from "CSP";

        CSP.Log("Testing CSP.Log");

    )xx";

	Avatar->GetScript()->SetScriptSource(AvatarScriptText.c_str());
	Avatar->GetScript()->Invoke();

	std::string AvatarOKOScriptText = R"xx(

        import * as OKO from "OKO";

        OKO.Log("Testing OKO.Log");

    )xx";

	Avatar->GetScript()->SetScriptSource(AvatarScriptText.c_str());
	Avatar->GetScript()->Invoke();

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_MULTIPLAYER_CONNECTION_INTERRUPT_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, ConnectionInterruptTest)
{
	InitialiseFoundationWithUserAgentInfo(EndpointBaseURI);

	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();
	auto* AssetSystem	 = SystemsManager.GetAssetSystem();

	const char* TestSpaceName			= "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription	= "OLY-UNITTEST-SPACEDESC-REWIND";
	const char* TestAssetCollectionName = "OLY-UNITTEST-ASSETCOLLECTION-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	char UniqueAssetCollectionName[256];
	SPRINTF(UniqueAssetCollectionName, "%s-%s", TestAssetCollectionName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto* Connection = new csp::multiplayer::MultiplayerConnection(Space.Id);

	bool Interrupted  = false;
	bool Disconnected = false;

	Connection->SetNetworkInterruptionCallback(
		[&Interrupted](csp::common::String Message)
		{
			Interrupted = true;
		});

	Connection->SetDisconnectionCallback(
		[&Disconnected](csp::common::String Message)
		{
			Disconnected = true;
		});

	auto [Ok] = Awaitable(&MultiplayerConnection::Connect, Connection).Await();

	EXPECT_TRUE(Ok);

	std::tie(Ok) = Awaitable(&MultiplayerConnection::InitialiseConnection, Connection).Await();

	EXPECT_TRUE(Ok);

	EntitySystem = Connection->GetSpaceEntitySystem();

	csp::common::String UserName = "Player 1";
	SpaceTransform UserTransform
		= {csp::common::Vector3 {1.452322f, 2.34f, 3.45f}, csp::common::Vector4 {4.1f, 5.1f, 6.1f, 7.1f}, csp::common::Vector3 {1, 1, 1}};
	AvatarState UserAvatarState		  = AvatarState::Idle;
	csp::common::String UserAvatarId  = "MyCoolAvatar";
	AvatarPlayMode UserAvatarPlayMode = AvatarPlayMode::Default;

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	auto [Avatar]
		= Awaitable(&SpaceEntitySystem::CreateAvatar, EntitySystem, UserName, UserTransform, UserAvatarState, UserAvatarId, UserAvatarPlayMode)
			  .Await();

	auto Start	   = std::chrono::steady_clock::now();
	auto Current   = std::chrono::steady_clock::now();
	float TestTime = 0;

	// Interrupt connection here
	while (!Interrupted && TestTime < 60)
	{
		std::this_thread::sleep_for(50ms);

		SetRandomProperties(Avatar);

		Current	 = std::chrono::steady_clock::now();
		TestTime = std::chrono::duration_cast<std::chrono::seconds>(Current - Start).count();

		csp::CSPFoundation::Tick();
	}

	EXPECT_TRUE(Interrupted);

	Awaitable(&MultiplayerConnection::Disconnect, Connection).Await();

	EXPECT_TRUE(Disconnected);

	// Delete MultiplayerConnection
	delete Connection;

	// Delete space
	Awaitable(&csp::systems::SpaceSystem::DeleteSpace, SpaceSystem, Space).Await();

	// Log out
	Awaitable(&csp::systems::UserSystem::Logout, UserSystem).Await();
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_USE_PORTAL_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, UsePortalTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	const char* TestSpaceName2		  = "OLY-UNITTEST-SPACE-REWIND-2";
	const char* TestSpaceDescription2 = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	char UniqueSpaceName2[256];
	SPRINTF(UniqueSpaceName2, "%s-%s", TestSpaceName2, GetUniqueHexString().c_str());

	// Log in
	csp::common::String UserId;
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	csp::systems::Space Space2;
	CreateSpace(SpaceSystem, UniqueSpaceName2, TestSpaceDescription2, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space2);

	csp::common::String PortalSpaceID;

	const csp::common::String UserName		= "Player 1";
	const SpaceTransform UserTransform		= {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};
	const AvatarState UserAvatarState		= AvatarState::Idle;
	const csp::common::String UserAvatarId	= "MyCoolAvatar";
	const AvatarPlayMode UserAvatarPlayMode = AvatarPlayMode::Default;

	{

		auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
		EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
		Connection	 = EnterResult.GetConnection();
		EntitySystem = Connection->GetSpaceEntitySystem();

		// Ensure we're in the first space
		EXPECT_EQ(SpaceSystem->GetCurrentSpace().Id, Space.Id);

		// Create Avatar
		EntitySystem->SetEntityCreatedCallback(
			[](SpaceEntity* Entity)
			{
			});

		auto [Avatar] = AWAIT(EntitySystem, CreateAvatar, UserName, UserTransform, UserAvatarState, UserAvatarId, UserAvatarPlayMode);

		// Create object to represent the portal
		csp::common::String ObjectName = "Object 1";
		SpaceTransform ObjectTransform = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};
		auto [CreatedObject]		   = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

		// Create portal component
		auto* PortalComponent = (PortalSpaceComponent*) CreatedObject->AddComponent(ComponentType::Portal);
		PortalComponent->SetSpaceId(Space2.Id);

		PortalSpaceID = PortalComponent->GetSpaceId();

		AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);
	}

	/*
		User would now interact with the portal
	*/

	{
		auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
		EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
		Connection	 = EnterResult.GetConnection();
		EntitySystem = Connection->GetSpaceEntitySystem();

		// Create Avatar
		EntitySystem->SetEntityCreatedCallback(
			[](SpaceEntity* Entity)
			{
			});

		auto [Avatar] = AWAIT(EntitySystem, CreateAvatar, UserName, UserTransform, UserAvatarState, UserAvatarId, UserAvatarPlayMode);

		AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);
	}

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);
	DeleteSpace(SpaceSystem, Space2.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_PORTAL_SCRIPT_INTERFACE_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, PortalScriptInterfaceTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	// Log in
	csp::common::String UserId;
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	// Create object to represent the portal
	csp::common::String ObjectName = "Object 1";
	SpaceTransform ObjectTransform = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};
	auto [CreatedObject]		   = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

	// Create portal component
	auto* PortalComponent = (PortalSpaceComponent*) CreatedObject->AddComponent(ComponentType::Portal);

	auto InitialPosition = csp::common::Vector3 {1.1f, 2.2f, 3.3f};
	PortalComponent->SetSpaceId("initialTestSpaceId");
	PortalComponent->SetIsEnabled(false);
	PortalComponent->SetPosition(InitialPosition);
	PortalComponent->SetRadius(123.123f);

	CreatedObject->QueueUpdate();
	EntitySystem->ProcessPendingEntityOperations();

	EXPECT_EQ(PortalComponent->GetSpaceId(), "initialTestSpaceId");
	EXPECT_EQ(PortalComponent->GetIsEnabled(), false);
	EXPECT_FLOAT_EQ(PortalComponent->GetPosition().X, InitialPosition.X);
	EXPECT_FLOAT_EQ(PortalComponent->GetPosition().Y, InitialPosition.Y);
	EXPECT_FLOAT_EQ(PortalComponent->GetPosition().Z, InitialPosition.Z);
	EXPECT_EQ(PortalComponent->GetRadius(), 123.123f);

	// Setup script
	std::string PortalScriptText = R"xx(
		var portal = ThisEntity.getPortalComponents()[0];
		portal.spaceId = "secondTestSpaceId";
		portal.isEnabled = true;
		portal.position = [4.4, 5.5, 6.6];
		portal.radius = 456.456;
    )xx";

	CreatedObject->GetScript()->SetScriptSource(PortalScriptText.c_str());
	CreatedObject->GetScript()->Invoke();

	EntitySystem->ProcessPendingEntityOperations();

	EXPECT_EQ(PortalComponent->GetSpaceId(), "secondTestSpaceId");
	EXPECT_EQ(PortalComponent->GetIsEnabled(), true);
	EXPECT_FLOAT_EQ(PortalComponent->GetPosition().X, 4.4f);
	EXPECT_FLOAT_EQ(PortalComponent->GetPosition().Y, 5.5f);
	EXPECT_FLOAT_EQ(PortalComponent->GetPosition().Z, 6.6f);
	EXPECT_FLOAT_EQ(PortalComponent->GetRadius(), 456.456f);

	// Cleanup
	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete MultiplayerConnection
	delete Connection;

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}

#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_PORTAL_THUMBNAIL_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, PortalThumbnailTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();
	auto* AssetSystem	 = SystemsManager.GetAssetSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	// Log in
	csp::common::String UserId;
	LogIn(UserSystem, UserId);

	auto FilePath = std::filesystem::absolute("assets/OKO.png");

	csp::systems::FileAssetDataSource Source;
	Source.FilePath = FilePath.string().c_str();

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, Source, Space);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	// Create object to represent the portal
	csp::common::String ObjectName = "Object 1";
	SpaceTransform ObjectTransform = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};
	auto [CreatedObject]		   = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

	// Create portal component
	auto* PortalComponent = (PortalSpaceComponent*) CreatedObject->AddComponent(ComponentType::Portal);

	// Get Thumbnail
	bool HasThumbailResult = false;

	csp::systems::UriResultCallback Callback = [&HasThumbailResult](const csp::systems::UriResult& Result)
	{
		if (Result.GetResultCode() == csp::services::EResultCode::Success)
		{
			HasThumbailResult = true;
			EXPECT_TRUE(Result.GetUri() != "");
		}
	};

	PortalComponent->SetSpaceId(Space.Id);
	PortalComponent->GetSpaceThumbnail(Callback);

	auto Start	   = std::chrono::steady_clock::now();
	auto Current   = std::chrono::steady_clock::now();
	float TestTime = 0;

	while (!HasThumbailResult && TestTime < 20)
	{
		std::this_thread::sleep_for(50ms);

		Current	 = std::chrono::steady_clock::now();
		TestTime = std::chrono::duration_cast<std::chrono::seconds>(Current - Start).count();
	}

	EXPECT_TRUE(HasThumbailResult);

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_DELETE_MULTIPLE_ENTITIES_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, DeleteMultipleEntitiesTest)
{
	// Test for OB-1046
	// If the rate limiter hasn't processed all PendingOutgoingUpdates after SpaceEntity deletion it will crash when trying to process them

	InitialiseFoundationWithUserAgentInfo(EndpointBaseURI);

	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	// Log in
	csp::common::String UserId;
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	// Create 3 seperate objects to ensure there is too many updates for the rate limiter to process in one tick

	// Create object
	const csp::common::String ObjectName = "Object 1";
	SpaceTransform ObjectTransform		 = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};

	auto [CreatedObject] = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);
	auto* ImageComponent = (ImageSpaceComponent*) CreatedObject->AddComponent(ComponentType::Image);
	CreatedObject->QueueUpdate();

	// Create object 2
	auto [CreatedObject2] = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);
	auto* ImageComponent2 = (ImageSpaceComponent*) CreatedObject2->AddComponent(ComponentType::Image);
	CreatedObject2->QueueUpdate();

	// Create object 3
	auto [CreatedObject3] = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);
	auto* ImageComponent3 = (ImageSpaceComponent*) CreatedObject3->AddComponent(ComponentType::Image);
	CreatedObject3->QueueUpdate();

	// Destroy Entites
	EntitySystem->DestroyEntity(CreatedObject,
								[](bool)
								{
								});
	EntitySystem->DestroyEntity(CreatedObject2,
								[](bool)
								{
								});
	EntitySystem->DestroyEntity(CreatedObject3,
								[](bool)
								{
								});

	csp::CSPFoundation::Tick();

	// Delete MultiplayerConnection
	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_ENTITY_SELECTION_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, EntitySelectionTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	const csp::common::String& UserName = "Player 1";
	const SpaceTransform& UserTransform
		= {csp::common::Vector3 {1.452322f, 2.34f, 3.45f}, csp::common::Vector4 {4.1f, 5.1f, 6.1f, 7.1f}, csp::common::Vector3 {1, 1, 1}};
	AvatarState UserAvatarState				= AvatarState::Idle;
	const csp::common::String& UserAvatarId = "MyCoolAvatar";
	AvatarPlayMode UserAvatarPlayMode		= AvatarPlayMode::Default;

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	auto [Avatar] = AWAIT(EntitySystem, CreateAvatar, UserName, UserTransform, UserAvatarState, UserAvatarId, UserAvatarPlayMode);
	EXPECT_NE(Avatar, nullptr);

	csp::common::String ObjectName = "Object 1";
	SpaceTransform ObjectTransform
		= {csp::common::Vector3 {1.452322f, 2.34f, 3.45f}, csp::common::Vector4 {4.1f, 5.1f, 6.1f, 7.1f}, csp::common::Vector3 {1, 1, 1}};

	auto [CreatedObject] = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

	CreatedObject->Select();

	EXPECT_TRUE(CreatedObject->IsSelected());

	CreatedObject->Deselect();

	EXPECT_FALSE(CreatedObject->IsSelected());

	// Delete MultiplayerConnection
	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_ASSET_PROCESSED_CALLBACK_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, AssetProcessedCallbackTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();
	auto* AssetSystem	 = SystemsManager.GetAssetSystem();

	const char* TestSpaceName			= "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription	= "OLY-UNITTEST-SPACEDESC-REWIND";
	const char* TestAssetCollectionName = "OLY-UNITTEST-ASSETCOLLECTION-REWIND";
	const char* TestAssetName			= "OLY-UNITTEST-ASSET-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	char UniqueAssetCollectionName[256];
	SPRINTF(UniqueAssetCollectionName, "%s-%s", TestAssetCollectionName, GetUniqueHexString().c_str());

	char UniqueAssetName[256];
	SPRINTF(UniqueAssetName, "%s-%s", TestAssetName, GetUniqueHexString().c_str());

	// Log in
	csp::common::String UserId;
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	// Setup Asset callback
	bool AssetDetailBlobChangedCallbackCalled = false;
	csp::common::String CallbackAssetId;

	auto AssetDetailBlobChangedCallback
		= [&AssetDetailBlobChangedCallbackCalled, &CallbackAssetId](const csp::multiplayer::AssetDetailBlobParams& Params)
	{
		if (AssetDetailBlobChangedCallbackCalled)
		{
			return;
		}

		EXPECT_EQ(Params.ChangeType, EAssetChangeType::Created);
		EXPECT_EQ(Params.AssetType, csp::systems::EAssetType::MODEL);

		CallbackAssetId						 = Params.AssetId;
		AssetDetailBlobChangedCallbackCalled = true;
	};

	Connection->SetAssetDetailBlobChangedCallback(AssetDetailBlobChangedCallback);

	// Create asset collection
	csp::systems::AssetCollection AssetCollection;
	CreateAssetCollection(AssetSystem, Space.Id, nullptr, UniqueAssetCollectionName, nullptr, nullptr, AssetCollection);

	// Create asset
	csp::systems::Asset Asset;
	CreateAsset(AssetSystem, AssetCollection, UniqueAssetName, nullptr, nullptr, Asset);

	// Upload data
	auto FilePath = std::filesystem::absolute("assets/test.json");
	csp::systems::FileAssetDataSource Source;
	Source.FilePath = FilePath.u8string().c_str();

	Source.SetMimeType("application/json");

	csp::common::String Uri;
	UploadAssetData(AssetSystem, AssetCollection, Asset, Source, Uri);

	// Wait for message
	auto Start	   = std::chrono::steady_clock::now();
	auto Current   = std::chrono::steady_clock::now();
	float TestTime = 0;

	while (!AssetDetailBlobChangedCallbackCalled && TestTime < 20)
	{
		std::this_thread::sleep_for(50ms);

		Current	 = std::chrono::steady_clock::now();
		TestTime = std::chrono::duration_cast<std::chrono::seconds>(Current - Start).count();
	}

	EXPECT_TRUE(AssetDetailBlobChangedCallbackCalled);
	EXPECT_EQ(CallbackAssetId, Asset.Id);

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_ASSET_PROCESS_GRACEFUL_FAILURE_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, AssetProcessGracefulFailureCallbackTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();
	auto* AssetSystem	 = SystemsManager.GetAssetSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	// Log in
	csp::common::String UserId;
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	// Setup Asset callback
	bool AssetDetailBlobChangedCallbackCalled = false;

	auto AssetDetailBlobChangedCallback = [&AssetDetailBlobChangedCallbackCalled](const csp::multiplayer::AssetDetailBlobParams& Params)
	{
		if (AssetDetailBlobChangedCallbackCalled)
		{
			return;
		}

		EXPECT_EQ(Params.ChangeType, EAssetChangeType::Invalid);
		EXPECT_EQ(Params.AssetType, csp::systems::EAssetType::IMAGE);

		AssetDetailBlobChangedCallbackCalled = true;
	};

	Connection->SetAssetDetailBlobChangedCallback(AssetDetailBlobChangedCallback);

	ReplicatedValue Param1 = static_cast<int64_t>(EAssetChangeType::Invalid);
	ReplicatedValue Param2 = "";
	ReplicatedValue Param3 = "";
	ReplicatedValue Param4 = "";
	ReplicatedValue Param5 = "";

	Connection->SendNetworkEventToClient("AssetDetailBlobChanged",
										 {Param1, Param2, Param3, Param4, Param5},
										 Connection->GetClientId(),
										 [](bool ok)
										 {
											 EXPECT_TRUE(ok);
										 });

	// Wait for message
	auto Start	   = std::chrono::steady_clock::now();
	auto Current   = std::chrono::steady_clock::now();
	float TestTime = 0;

	while (!AssetDetailBlobChangedCallbackCalled && TestTime < 20)
	{
		std::this_thread::sleep_for(50ms);

		Current	 = std::chrono::steady_clock::now();
		TestTime = std::chrono::duration_cast<std::chrono::seconds>(Current - Start).count();
	}

	EXPECT_TRUE(AssetDetailBlobChangedCallbackCalled);

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_DELETE_SCRIPT_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, DeleteScriptTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	// Log in
	csp::common::String UserId;
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	const std::string ScriptText = R"xx(
		
        var entities = TheEntitySystem.getEntities();
		var entityIndex = TheEntitySystem.getIndexOfEntity(ThisEntity.id);

		globalThis.onTick = () => {
			var entity = entities[entityIndex];
			entity.position = [10, 10, 10];
		}
 
		ThisEntity.subscribeToMessage("entityTick", "onTick");
		  
    )xx";

	// Create object
	const csp::common::String ObjectName = "Object 1";
	SpaceTransform ObjectTransform		 = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};

	auto [CreatedObject] = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

	// Create script
	auto* ScriptComponent = static_cast<ScriptSpaceComponent*>(CreatedObject->AddComponent(ComponentType::ScriptData));
	ScriptComponent->SetScriptSource(csp::common::String(ScriptText.c_str()));
	CreatedObject->GetScript()->Invoke();

	CreatedObject->QueueUpdate();
	EntitySystem->ProcessPendingEntityOperations();

	// Ensure position is set to 0
	EXPECT_EQ(CreatedObject->GetPosition(), csp::common::Vector3::Zero());

	// Delete script component
	CreatedObject->RemoveComponent(ScriptComponent->GetId());

	CreatedObject->QueueUpdate();
	EntitySystem->ProcessPendingEntityOperations();

	// Tick to attempt to call scripts tick event
	csp::CSPFoundation::Tick();

	CreatedObject->QueueUpdate();
	EntitySystem->ProcessPendingEntityOperations();

	// Ensure position is still set to 0
	EXPECT_EQ(CreatedObject->GetPosition(), csp::common::Vector3::Zero());

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_DELETE_AND_CHANGE_COMPONENT_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, DeleteAndChangeComponentTest)
{
	// Test for: OB-864
	// Second script deletion test adds a second component to the object with the script
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	// Log in
	csp::common::String UserId;
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	const std::string ScriptText = R"xx(
		
        var entities = TheEntitySystem.getEntities();
		var entityIndex = TheEntitySystem.getIndexOfEntity(ThisEntity.id);

		globalThis.onTick = () => {
			var entity = entities[entityIndex];
			entity.position = [10, 10, 10];
		}
 
		ThisEntity.subscribeToMessage("entityTick", "onTick");
		  
    )xx";

	// Create object
	const csp::common::String ObjectName = "Object 1";
	SpaceTransform ObjectTransform		 = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};

	auto [CreatedObject] = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

	// Create animated model component
	auto* AnimatedComponent = static_cast<AnimatedModelSpaceComponent*>(CreatedObject->AddComponent(ComponentType::AnimatedModel));

	// Create script
	auto* ScriptComponent = static_cast<ScriptSpaceComponent*>(CreatedObject->AddComponent(ComponentType::ScriptData));
	ScriptComponent->SetScriptSource(csp::common::String(ScriptText.c_str()));
	CreatedObject->GetScript()->Invoke();

	CreatedObject->QueueUpdate();
	EntitySystem->ProcessPendingEntityOperations();

	// Make a component update
	AnimatedComponent->SetPosition(csp::common::Vector3::One());

	// Delete script component
	CreatedObject->RemoveComponent(ScriptComponent->GetId());

	CreatedObject->QueueUpdate();
	EntitySystem->ProcessPendingEntityOperations();

	// Ensure entity update doesn't crash
	csp::CSPFoundation::Tick();

	CreatedObject->QueueUpdate();
	EntitySystem->ProcessPendingEntityOperations();

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_ADD_SECOND_SCRIPT_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, AddSecondScriptTest)
{
	// Test for OB-1407
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	// Log in
	csp::common::String UserId;
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	std::atomic_bool ScriptSystemReady = false;

	auto EntityCreatedCallback = [](SpaceEntity* Entity)
	{
		std::cerr << "EntityCreatedCallback called" << std::endl;
	};

	auto EntitiesReadyCallback = [](bool Ok)
	{
		EXPECT_EQ(Ok, true);
		std::cerr << "EntitiesReadyCallback called" << std::endl;
	};

	auto ScriptSystemReadyCallback = [&ScriptSystemReady](bool Ok)
	{
		EXPECT_EQ(Ok, true);
		std::cerr << "ScriptSystemReadyCallback called" << std::endl;
		ScriptSystemReady = true;
	};

	SpaceSystem->SetEntityCreatedCallback(EntityCreatedCallback);
	SpaceSystem->SetInitialEntitiesRetrievedCallback(EntitiesReadyCallback);
	SpaceSystem->SetScriptSystemReadyCallback(ScriptSystemReadyCallback);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();
	OnConnect();

	const std::string ScriptText = R"xx(
		
        var entities = TheEntitySystem.getEntities();
		var entityIndex = TheEntitySystem.getIndexOfEntity(ThisEntity.id);

		globalThis.onTick = () => {
			var entity = entities[entityIndex];
			entity.position = [1, 1, 1];
		}
 
		ThisEntity.subscribeToMessage("entityTick", "onTick");
		  
    )xx";

	auto ScriptSystemIsReady = [&ScriptSystemReady]()
	{
		std::cerr << "Waiting for ScriptSystemReady" << std::endl;
		return (ScriptSystemReady == true);
	};

	EXPECT_EQ(ResponseWaiter::WaitFor(ScriptSystemIsReady, std::chrono::seconds(5)), true);

	// Create object
	const csp::common::String ObjectName = "Object 1";
	SpaceTransform ObjectTransform		 = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};

	auto [CreatedObject] = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

	bool PatchPending = true;
	CreatedObject->SetPatchSentCallback(
		[&PatchPending](bool ok)
		{
			PatchPending = false;
		});

	// Create script
	auto* ScriptComponent = static_cast<ScriptSpaceComponent*>(CreatedObject->AddComponent(ComponentType::ScriptData));
	ScriptComponent->SetScriptSource(csp::common::String(ScriptText.c_str()));
	CreatedObject->GetScript()->Invoke();

	CreatedObject->QueueUpdate();

	while (PatchPending)
	{
		EntitySystem->ProcessPendingEntityOperations();
		std::this_thread::sleep_for(10ms);
	}

	PatchPending = true;

	// Delete script component
	CreatedObject->RemoveComponent(ScriptComponent->GetId());

	CreatedObject->QueueUpdate();

	while (PatchPending)
	{
		EntitySystem->ProcessPendingEntityOperations();
		std::this_thread::sleep_for(10ms);
	}

	PatchPending = true;

	// Ensure position is set to 0
	EXPECT_EQ(CreatedObject->GetPosition(), csp::common::Vector3::Zero());

	// Re-add script component
	ScriptComponent = static_cast<ScriptSpaceComponent*>(CreatedObject->AddComponent(ComponentType::ScriptData));
	ScriptComponent->SetScriptSource(csp::common::String(ScriptText.c_str()));
	CreatedObject->GetScript()->Invoke();

	CreatedObject->QueueUpdate();

	while (PatchPending)
	{
		EntitySystem->ProcessPendingEntityOperations();
		csp::CSPFoundation::Tick();
		std::this_thread::sleep_for(10ms);
	}

	EXPECT_EQ(CreatedObject->GetPosition(), csp::common::Vector3::One());

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_CONVERSATION_COMPONENT_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, ConversationComponentTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	const char* TestSpaceName2		  = "OLY-UNITTEST-SPACE-REWIND-2";
	const char* TestSpaceDescription2 = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	char UniqueSpaceName2[256];
	SPRINTF(UniqueSpaceName2, "%s-%s", TestSpaceName2, GetUniqueHexString().c_str());

	// Log in
	csp::common::String UserId;
	LogIn(UserSystem, UserId);
	const auto UserDisplayName = GetFullProfileByUserId(UserSystem, UserId).DisplayName;

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	const csp::common::String UserName		= "Player 1";
	const SpaceTransform UserTransform		= {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};
	const AvatarState UserAvatarState		= AvatarState::Idle;
	const csp::common::String UserAvatarId	= "MyCoolAvatar";
	const AvatarPlayMode UserAvatarPlayMode = AvatarPlayMode::Default;

	{

		auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
		EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
		Connection	 = EnterResult.GetConnection();
		EntitySystem = Connection->GetSpaceEntitySystem();

		// Ensure we're in the first space
		EXPECT_EQ(SpaceSystem->GetCurrentSpace().Id, Space.Id);

		// Create Avatar
		EntitySystem->SetEntityCreatedCallback(
			[](SpaceEntity* Entity)
			{
			});

		auto [Avatar] = AWAIT(EntitySystem, CreateAvatar, UserName, UserTransform, UserAvatarState, UserAvatarId, UserAvatarPlayMode);

		// Create object to represent the conversation
		csp::common::String ObjectName = "Object 1";
		SpaceTransform ObjectTransform = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};
		auto [CreatedObject]		   = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

		// Create conversation component
		auto* ConversationComponent = (ConversationSpaceComponent*) CreatedObject->AddComponent(ComponentType::Conversation);

		EXPECT_EQ(ConversationComponent->GetIsVisible(), true);
		EXPECT_EQ(ConversationComponent->GetIsActive(), true);

		ConversationComponent->SetIsActive(false);
		ConversationComponent->SetIsVisible(false);

		EXPECT_EQ(ConversationComponent->GetIsVisible(), false);
		EXPECT_EQ(ConversationComponent->GetIsActive(), false);

		SpaceTransform DefaultTransform = SpaceTransform();

		EXPECT_EQ(ConversationComponent->GetPosition().X, DefaultTransform.Position.X);
		EXPECT_EQ(ConversationComponent->GetPosition().Y, DefaultTransform.Position.Y);
		EXPECT_EQ(ConversationComponent->GetPosition().Z, DefaultTransform.Position.Z);

		csp::common::Vector3 NewPosition(1, 2, 3);
		ConversationComponent->SetPosition(NewPosition);

		EXPECT_EQ(ConversationComponent->GetPosition().X, NewPosition.X);
		EXPECT_EQ(ConversationComponent->GetPosition().Y, NewPosition.Y);
		EXPECT_EQ(ConversationComponent->GetPosition().Z, NewPosition.Z);

		EXPECT_EQ(ConversationComponent->GetRotation().W, DefaultTransform.Rotation.W);
		EXPECT_EQ(ConversationComponent->GetRotation().X, DefaultTransform.Rotation.X);
		EXPECT_EQ(ConversationComponent->GetRotation().Y, DefaultTransform.Rotation.Y);
		EXPECT_EQ(ConversationComponent->GetRotation().Z, DefaultTransform.Rotation.Z);

		csp::common::Vector4 NewRotation(4, 5, 6, 7);
		ConversationComponent->SetRotation(NewRotation);

		EXPECT_EQ(ConversationComponent->GetRotation().W, NewRotation.W);
		EXPECT_EQ(ConversationComponent->GetRotation().X, NewRotation.X);
		EXPECT_EQ(ConversationComponent->GetRotation().Y, NewRotation.Y);
		EXPECT_EQ(ConversationComponent->GetRotation().Z, NewRotation.Z);

		EXPECT_EQ(ConversationComponent->GetTitle(), "");
		EXPECT_EQ(ConversationComponent->GetDate(), "");
		EXPECT_EQ(ConversationComponent->GetNumberOfReplies(), 0);

		ConversationComponent->SetTitle("TestTitle");
		ConversationComponent->SetDate("02-01-1972");
		ConversationComponent->SetNumberOfReplies(2);

		EXPECT_EQ(ConversationComponent->GetTitle(), "TestTitle");
		EXPECT_EQ(ConversationComponent->GetDate(), "02-01-1972");
		EXPECT_EQ(ConversationComponent->GetNumberOfReplies(), 2);

		csp::common::String ConversationId;
		csp::common::String MessageId;
		{
			auto [Result] = AWAIT(ConversationComponent, CreateConversation, "TestMessage");
			EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);
			EXPECT_TRUE(Result.GetValue() != "");
			ConversationId = Result.GetValue();
		}

		{
			auto [Result] = AWAIT_PRE(ConversationComponent, AddMessage, RequestPredicate, "Test");
			EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);
			MessageId = Result.GetMessageInfo().Id;
			EXPECT_EQ(Result.GetMessageInfo().Edited, false);
		}

		{
			auto [Result] = AWAIT(ConversationComponent, GetMessageInfo, MessageId);
			EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);
			EXPECT_EQ(Result.GetMessageInfo().Edited, false);
		}

		{
			MessageInfo NewData = MessageInfo();
			NewData.Message		= "NewTest";
			auto [Result]		= AWAIT(ConversationComponent, SetMessageInfo, MessageId, NewData);
			EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);
			EXPECT_EQ(Result.GetMessageInfo().Edited, true);
		}

		{
			auto [Result] = AWAIT(ConversationComponent, GetConversationInfo);
			EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);
			EXPECT_EQ(Result.GetConversationInfo().UserID, UserId);
			EXPECT_EQ(Result.GetConversationInfo().UserDisplayName, UserDisplayName);
			EXPECT_EQ(Result.GetConversationInfo().Message, "TestMessage");
			EXPECT_FALSE(Result.GetConversationInfo().Edited);
			EXPECT_FALSE(Result.GetConversationInfo().Resolved);

			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Position.X, DefaultTransform.Position.X);
			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Position.Y, DefaultTransform.Position.Y);
			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Position.Z, DefaultTransform.Position.Z);

			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Rotation.W, DefaultTransform.Rotation.W);
			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Rotation.X, DefaultTransform.Rotation.X);
			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Rotation.Y, DefaultTransform.Rotation.Y);
			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Rotation.Z, DefaultTransform.Rotation.Z);

			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Scale.X, DefaultTransform.Scale.X);
			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Scale.Y, DefaultTransform.Scale.Y);
			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Scale.Z, DefaultTransform.Scale.Z);
		}

		{
			ConversationInfo NewData = ConversationInfo();
			SpaceTransform CameraTransformValue
				= SpaceTransform(csp::common::Vector3().One(), csp::common::Vector4().One(), csp::common::Vector3().One());
			NewData.Resolved	   = true;
			NewData.CameraPosition = CameraTransformValue;
			NewData.Message		   = "TestMessage1";

			auto [Result] = AWAIT(ConversationComponent, SetConversationInfo, NewData);
			EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);
			EXPECT_EQ(Result.GetConversationInfo().UserID, UserId);
			EXPECT_EQ(Result.GetConversationInfo().UserDisplayName, UserDisplayName);
			EXPECT_EQ(Result.GetConversationInfo().Message, "TestMessage1");
			EXPECT_TRUE(Result.GetConversationInfo().Edited);
			EXPECT_TRUE(Result.GetConversationInfo().Resolved);

			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Position.X, CameraTransformValue.Position.X);
			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Position.Y, CameraTransformValue.Position.Y);
			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Position.Z, CameraTransformValue.Position.Z);

			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Rotation.W, CameraTransformValue.Rotation.W);
			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Rotation.X, CameraTransformValue.Rotation.X);
			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Rotation.Y, CameraTransformValue.Rotation.Y);
			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Rotation.Z, CameraTransformValue.Rotation.Z);

			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Scale.X, CameraTransformValue.Scale.X);
			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Scale.Y, CameraTransformValue.Scale.Y);
			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Scale.Z, CameraTransformValue.Scale.Z);
			EXPECT_EQ(Result.GetConversationInfo().Message, "TestMessage1");
		}

		auto TestMessage = "test123";
		Connection->ListenNetworkEvent("ConversationSystem:NewMessage",
									   [=](bool ok, csp::common::Array<ReplicatedValue> Data)
									   {
										   EXPECT_TRUE(ok);
										   ConversationId == Data[0].GetString();
										   std::cerr << "Test Event Received " << ok << std::endl;
									   });

		{
			auto [Result] = AWAIT_PRE(ConversationComponent, AddMessage, RequestPredicate, TestMessage);

			EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);
			MessageId = Result.GetMessageInfo().Id;
		}
		{
			auto [Result] = AWAIT(ConversationComponent, GetAllMessages);

			EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);
			EXPECT_EQ(Result.GetTotalCount(), 2);

			EXPECT_EQ(Result.GetMessages()[0].Id, MessageId);
		}
		{
			auto [Result] = AWAIT(ConversationComponent, GetMessage, MessageId);

			EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);
			EXPECT_EQ(Result.GetMessageInfo().Id, MessageId);
		}

		{
			auto [Result] = AWAIT(ConversationComponent, DeleteMessage, MessageId);

			EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);
		}

		{
			auto [Result] = AWAIT(ConversationComponent, DeleteConversation);

			EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);
		}

		AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);
	}

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_CONVERSATION_COMPONENT_MOVE_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, ConversationComponentMoveTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	const char* TestSpaceName2		  = "OLY-UNITTEST-SPACE-REWIND-2";
	const char* TestSpaceDescription2 = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	// Log in
	csp::common::String UserId;
	LogIn(UserSystem, UserId);
	const auto UserDisplayName = GetFullProfileByUserId(UserSystem, UserId).DisplayName;

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	{

		auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
		EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
		Connection	 = EnterResult.GetConnection();
		EntitySystem = Connection->GetSpaceEntitySystem();

		// Ensure we're in the first space
		EXPECT_EQ(SpaceSystem->GetCurrentSpace().Id, Space.Id);

		// Create Avatar
		EntitySystem->SetEntityCreatedCallback(
			[](SpaceEntity* Entity)
			{
			});

		csp::common::String ObjectName1 = "Object 1";
		csp::common::String ObjectName2 = "Object 2";

		SpaceTransform ObjectTransform = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};

		auto [CreatedObject1] = AWAIT(EntitySystem, CreateObject, ObjectName1, ObjectTransform);
		auto [CreatedObject2] = AWAIT(EntitySystem, CreateObject, ObjectName2, ObjectTransform);

		// Create conversation component
		auto* ConversationComponent1 = (ConversationSpaceComponent*) CreatedObject1->AddComponent(ComponentType::Conversation);
		auto* ConversationComponent2 = (ConversationSpaceComponent*) CreatedObject2->AddComponent(ComponentType::Conversation);


		csp::common::String ConversationId;
		csp::common::String MessageId;
		{
			auto [Result] = AWAIT(ConversationComponent1, CreateConversation, "TestMessage");
			EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);
			EXPECT_TRUE(Result.GetValue() != "");
			ConversationId = Result.GetValue();
		}

		SpaceTransform DefaultTransform = SpaceTransform();

		{
			auto [Result] = AWAIT(ConversationComponent1, GetConversationInfo);
			EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);
			EXPECT_EQ(Result.GetConversationInfo().UserID, UserId);
			EXPECT_EQ(Result.GetConversationInfo().UserDisplayName, UserDisplayName);
			EXPECT_EQ(Result.GetConversationInfo().Message, "TestMessage");
			EXPECT_FALSE(Result.GetConversationInfo().Edited);
			EXPECT_FALSE(Result.GetConversationInfo().Resolved);

			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Position.X, DefaultTransform.Position.X);
			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Position.Y, DefaultTransform.Position.Y);
			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Position.Z, DefaultTransform.Position.Z);

			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Rotation.W, DefaultTransform.Rotation.W);
			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Rotation.X, DefaultTransform.Rotation.X);
			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Rotation.Y, DefaultTransform.Rotation.Y);
			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Rotation.Z, DefaultTransform.Rotation.Z);

			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Scale.X, DefaultTransform.Scale.X);
			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Scale.Y, DefaultTransform.Scale.Y);
			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Scale.Z, DefaultTransform.Scale.Z);
		}

		{
			auto [Result] = AWAIT(ConversationComponent2, GetConversationInfo);
			EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Failed);
		}

		{
			auto Result = ConversationComponent2->MoveConversationFromComponent(*ConversationComponent1);
			EXPECT_TRUE(Result);
		}

		{
			auto [Result] = AWAIT(ConversationComponent1, GetConversationInfo);
			EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Failed);
		}

		{
			auto [Result] = AWAIT(ConversationComponent2, GetConversationInfo);
			EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);
			EXPECT_EQ(Result.GetConversationInfo().UserID, UserId);
			EXPECT_EQ(Result.GetConversationInfo().UserDisplayName, UserDisplayName);
			EXPECT_EQ(Result.GetConversationInfo().Message, "TestMessage");
			EXPECT_FALSE(Result.GetConversationInfo().Edited);
			EXPECT_FALSE(Result.GetConversationInfo().Resolved);

			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Position.X, DefaultTransform.Position.X);
			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Position.Y, DefaultTransform.Position.Y);
			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Position.Z, DefaultTransform.Position.Z);

			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Rotation.W, DefaultTransform.Rotation.W);
			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Rotation.X, DefaultTransform.Rotation.X);
			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Rotation.Y, DefaultTransform.Rotation.Y);
			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Rotation.Z, DefaultTransform.Rotation.Z);

			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Scale.X, DefaultTransform.Scale.X);
			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Scale.Y, DefaultTransform.Scale.Y);
			EXPECT_EQ(Result.GetConversationInfo().CameraPosition.Scale.Z, DefaultTransform.Scale.Z);
		}

		{
			auto [Result] = AWAIT(ConversationComponent2, DeleteConversation);
			EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);
		}

		AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);
	}

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_CONVERSATION_COMPONENT_SCRIPT_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, ConversationComponentScriptTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	// Log in
	csp::common::String UserId;
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	{
		auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
		EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
		Connection	 = EnterResult.GetConnection();
		EntitySystem = Connection->GetSpaceEntitySystem();

		EntitySystem->SetEntityCreatedCallback(
			[](SpaceEntity* Entity)
			{
			});

		// Create object to represent the conversation
		csp::common::String ObjectName = "Object 1";
		SpaceTransform ObjectTransform = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};
		auto [CreatedObject]		   = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

		// Create conversation component
		auto* ConversationComponent = (ConversationSpaceComponent*) CreatedObject->AddComponent(ComponentType::Conversation);

		SpaceTransform DefaultTransform = SpaceTransform();

		EXPECT_EQ(ConversationComponent->GetIsVisible(), true);
		EXPECT_EQ(ConversationComponent->GetIsActive(), true);

		EXPECT_EQ(ConversationComponent->GetPosition().X, DefaultTransform.Position.X);
		EXPECT_EQ(ConversationComponent->GetPosition().Y, DefaultTransform.Position.Y);
		EXPECT_EQ(ConversationComponent->GetPosition().Z, DefaultTransform.Position.Z);

		EXPECT_EQ(ConversationComponent->GetRotation().W, DefaultTransform.Rotation.W);
		EXPECT_EQ(ConversationComponent->GetRotation().X, DefaultTransform.Rotation.X);
		EXPECT_EQ(ConversationComponent->GetRotation().Y, DefaultTransform.Rotation.Y);
		EXPECT_EQ(ConversationComponent->GetRotation().Z, DefaultTransform.Rotation.Z);

		CreatedObject->QueueUpdate();
		EntitySystem->ProcessPendingEntityOperations();

		// Setup script
		std::string ConversationScriptText = R"xx(
			var conversation = ThisEntity.getConversationComponents()[0];
			conversation.isVisible = false;
			conversation.isActive = false;
			conversation.position = [1,2,3];
			conversation.rotation = [4,5,6,7];
		)xx";

		CreatedObject->GetScript()->SetScriptSource(ConversationScriptText.c_str());
		CreatedObject->GetScript()->Invoke();

		EntitySystem->ProcessPendingEntityOperations();

		EXPECT_FALSE(ConversationComponent->GetIsVisible());
		EXPECT_FALSE(ConversationComponent->GetIsActive());

		csp::common::Vector3 NewPosition(1, 2, 3);
		EXPECT_EQ(ConversationComponent->GetPosition().X, NewPosition.X);
		EXPECT_EQ(ConversationComponent->GetPosition().Y, NewPosition.Y);
		EXPECT_EQ(ConversationComponent->GetPosition().Z, NewPosition.Z);

		csp::common::Vector4 NewRotation(4, 5, 6, 7);
		EXPECT_EQ(ConversationComponent->GetRotation().W, NewRotation.W);
		EXPECT_EQ(ConversationComponent->GetRotation().X, NewRotation.X);
		EXPECT_EQ(ConversationComponent->GetRotation().Y, NewRotation.Y);
		EXPECT_EQ(ConversationComponent->GetRotation().Z, NewRotation.Z);
	};

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_AUDIO_COMPONENT_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, AudioComponentTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	// Log in
	csp::common::String UserId;
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	// Create object to represent the audio
	csp::common::String ObjectName = "Object 1";
	SpaceTransform ObjectTransform = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};
	auto [CreatedObject]		   = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

	// Create audio component
	auto* AudioComponent = static_cast<AudioSpaceComponent*>(CreatedObject->AddComponent(ComponentType::Audio));

	// Ensure defaults are set
	EXPECT_EQ(AudioComponent->GetPosition(), csp::common::Vector3::Zero());
	EXPECT_EQ(AudioComponent->GetPlaybackState(), AudioPlaybackState::Reset);
	EXPECT_EQ(AudioComponent->GetAudioType(), AudioType::Global);
	EXPECT_EQ(AudioComponent->GetAudioAssetId(), "");
	EXPECT_EQ(AudioComponent->GetAssetCollectionId(), "");
	EXPECT_EQ(AudioComponent->GetAttenuationRadius(), 10.f);
	EXPECT_EQ(AudioComponent->GetIsLoopPlayback(), false);
	EXPECT_EQ(AudioComponent->GetTimeSincePlay(), 0.f);
	EXPECT_EQ(AudioComponent->GetVolume(), 1.f);
	EXPECT_EQ(AudioComponent->GetIsEnabled(), true);

	// Set new values
	csp::common::String AssetId			  = "TEST_ASSET_ID";
	csp::common::String AssetCollectionId = "TEST_COLLECTION_ID";

	AudioComponent->SetPosition(csp::common::Vector3::One());
	AudioComponent->SetPlaybackState(AudioPlaybackState::Play);
	AudioComponent->SetAudioType(AudioType::Spatial);
	AudioComponent->SetAudioAssetId(AssetId);
	AudioComponent->SetAssetCollectionId(AssetCollectionId);
	AudioComponent->SetAttenuationRadius(100.f);
	AudioComponent->SetIsLoopPlayback(true);
	AudioComponent->SetTimeSincePlay(1.f);
	AudioComponent->SetVolume(0.5f);
	AudioComponent->SetIsEnabled(false);

	// Ensure values are set correctly
	EXPECT_EQ(AudioComponent->GetPosition(), csp::common::Vector3::One());
	EXPECT_EQ(AudioComponent->GetPlaybackState(), AudioPlaybackState::Play);
	EXPECT_EQ(AudioComponent->GetAudioType(), AudioType::Spatial);
	EXPECT_EQ(AudioComponent->GetAudioAssetId(), AssetId);
	EXPECT_EQ(AudioComponent->GetAssetCollectionId(), AssetCollectionId);
	EXPECT_EQ(AudioComponent->GetAttenuationRadius(), 100.f);
	EXPECT_EQ(AudioComponent->GetIsLoopPlayback(), true);
	EXPECT_EQ(AudioComponent->GetTimeSincePlay(), 1.f);
	EXPECT_EQ(AudioComponent->GetVolume(), 0.5f);
	EXPECT_EQ(AudioComponent->GetIsEnabled(), false);

	// Test invalid volume values
	AudioComponent->SetVolume(1.5f);
	EXPECT_EQ(AudioComponent->GetVolume(), 0.5f);
	AudioComponent->SetVolume(-2.5f);
	EXPECT_EQ(AudioComponent->GetVolume(), 0.5f);

	// Test boundary volume values
	AudioComponent->SetVolume(1.f);
	EXPECT_EQ(AudioComponent->GetVolume(), 1.f);
	AudioComponent->SetVolume(0.f);
	EXPECT_EQ(AudioComponent->GetVolume(), 0.f);

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif


#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_VIDEO_PLAYER_COMPONENT_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, VideoPlayerComponentTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	// Log in
	csp::common::String UserId;
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	// Create object to represent the audio
	csp::common::String ObjectName = "Object 1";
	SpaceTransform ObjectTransform = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};
	auto [CreatedObject]		   = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

	// Create audio component
	auto* VideoComponent = static_cast<VideoPlayerSpaceComponent*>(CreatedObject->AddComponent(ComponentType::VideoPlayer));

	// Ensure defaults are set
	EXPECT_EQ(VideoComponent->GetPosition(), csp::common::Vector3::Zero());
	EXPECT_EQ(VideoComponent->GetPlaybackState(), VideoPlayerPlaybackState::Reset);
	EXPECT_EQ(VideoComponent->GetVideoAssetURL(), "");
	EXPECT_EQ(VideoComponent->GetAssetCollectionId(), "");
	EXPECT_EQ(VideoComponent->GetAttenuationRadius(), 10.f);
	EXPECT_EQ(VideoComponent->GetIsLoopPlayback(), false);
	EXPECT_EQ(VideoComponent->GetTimeSincePlay(), 0.f);
	EXPECT_EQ(VideoComponent->GetIsStateShared(), false);
	EXPECT_EQ(VideoComponent->GetIsAutoPlay(), false);
	EXPECT_EQ(VideoComponent->GetIsAutoResize(), false);
	EXPECT_EQ(VideoComponent->GetCurrentPlayheadPosition(), 0.0f);
	EXPECT_EQ(VideoComponent->GetVideoPlayerSourceType(), VideoPlayerSourceType::AssetSource);
	EXPECT_EQ(VideoComponent->GetIsVisible(), true);
	EXPECT_EQ(VideoComponent->GetMeshComponentId(), 0);

	auto* ModelComponent = static_cast<VideoPlayerSpaceComponent*>(CreatedObject->AddComponent(ComponentType::AnimatedModel));

	CreatedObject->QueueUpdate();
	EntitySystem->ProcessPendingEntityOperations();

	// Set new values
	csp::common::String AssetId			  = "TEST_ASSET_ID";
	csp::common::String AssetCollectionId = "TEST_COLLECTION_ID";

	VideoComponent->SetPosition(csp::common::Vector3::One());
	VideoComponent->SetPlaybackState(VideoPlayerPlaybackState::Play);
	VideoComponent->SetVideoAssetURL("http://youtube.com/avideo");
	VideoComponent->SetAssetCollectionId(AssetId);
	VideoComponent->SetAttenuationRadius(100.f);
	VideoComponent->SetIsLoopPlayback(true);
	VideoComponent->SetTimeSincePlay(1.f);
	VideoComponent->SetIsStateShared(true);
	VideoComponent->SetIsAutoPlay(true);
	VideoComponent->SetIsAutoResize(true);
	VideoComponent->SetCurrentPlayheadPosition(1.0f);
	VideoComponent->SetVideoPlayerSourceType(VideoPlayerSourceType::URLSource);
	VideoComponent->SetIsVisible(false);
	VideoComponent->SetMeshComponentId(ModelComponent->GetId());

	// Ensure values are set correctly
	EXPECT_EQ(VideoComponent->GetPosition(), csp::common::Vector3::One());
	EXPECT_EQ(VideoComponent->GetPlaybackState(), VideoPlayerPlaybackState::Play);
	EXPECT_EQ(VideoComponent->GetVideoAssetURL(), "http://youtube.com/avideo");
	EXPECT_EQ(VideoComponent->GetAssetCollectionId(), AssetId);
	EXPECT_EQ(VideoComponent->GetAttenuationRadius(), 100.f);
	EXPECT_EQ(VideoComponent->GetIsLoopPlayback(), true);
	EXPECT_EQ(VideoComponent->GetTimeSincePlay(), 1.f);
	EXPECT_EQ(VideoComponent->GetIsStateShared(), true);
	EXPECT_EQ(VideoComponent->GetIsAutoPlay(), true);
	EXPECT_EQ(VideoComponent->GetIsAutoResize(), true);
	EXPECT_EQ(VideoComponent->GetCurrentPlayheadPosition(), 1.0f);
	EXPECT_EQ(VideoComponent->GetVideoPlayerSourceType(), VideoPlayerSourceType::URLSource);
	EXPECT_EQ(VideoComponent->GetIsVisible(), false);
	EXPECT_EQ(VideoComponent->GetMeshComponentId(), ModelComponent->GetId());

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_COLLISION_COMPONENT_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, CollisionComponentTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	// Log in
	csp::common::String UserId;
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	// Create object to represent the audio
	csp::common::String ObjectName = "Object 1";
	SpaceTransform ObjectTransform = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};
	auto [CreatedObject]		   = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

	// Create collision component
	auto* CollisionComponent = static_cast<CollisionSpaceComponent*>(CreatedObject->AddComponent(ComponentType::Collision));

	// Ensure defaults are set
	EXPECT_EQ(CollisionComponent->GetPosition(), csp::common::Vector3::Zero());
	EXPECT_EQ(CollisionComponent->GetRotation(), csp::common::Vector4(0, 0, 0, 1));
	EXPECT_EQ(CollisionComponent->GetScale(), csp::common::Vector3::One());
	EXPECT_EQ(CollisionComponent->GetUnscaledBoundingBoxMin(), csp::common::Vector3(-0.5, -0.5, -0.5));
	EXPECT_EQ(CollisionComponent->GetUnscaledBoundingBoxMax(), csp::common::Vector3(0.5, 0.5, 0.5));
	EXPECT_EQ(CollisionComponent->GetScaledBoundingBoxMin(), csp::common::Vector3(-0.5, -0.5, -0.5));
	EXPECT_EQ(CollisionComponent->GetScaledBoundingBoxMax(), csp::common::Vector3(0.5, 0.5, 0.5));
	EXPECT_EQ(CollisionComponent->GetCollisionMode(), csp::multiplayer::CollisionMode::Collision);
	EXPECT_EQ(CollisionComponent->GetCollisionShape(), csp::multiplayer::CollisionShape::Box);
	EXPECT_EQ(CollisionComponent->GetCollisionAssetId(), "");
	EXPECT_EQ(CollisionComponent->GetAssetCollectionId(), "");

	// Set new values

	CollisionComponent->SetPosition(csp::common::Vector3::One());
	CollisionComponent->SetScale(csp::common::Vector3(2, 2, 2));
	CollisionComponent->SetCollisionMode(csp::multiplayer::CollisionMode::Trigger);
	CollisionComponent->SetCollisionShape(csp::multiplayer::CollisionShape::Mesh);
	CollisionComponent->SetCollisionAssetId("TestAssetID");
	CollisionComponent->SetAssetCollectionId("TestAssetCollectionID");

	// Ensure values are set correctly
	EXPECT_EQ(CollisionComponent->GetPosition(), csp::common::Vector3::One());
	EXPECT_EQ(CollisionComponent->GetScale(), csp::common::Vector3(2, 2, 2));
	EXPECT_EQ(CollisionComponent->GetUnscaledBoundingBoxMin(), csp::common::Vector3(-0.5, -0.5, -0.5));
	EXPECT_EQ(CollisionComponent->GetUnscaledBoundingBoxMax(), csp::common::Vector3(0.5, 0.5, 0.5));
	EXPECT_EQ(CollisionComponent->GetScaledBoundingBoxMin(), csp::common::Vector3(-1, -1, -1));
	EXPECT_EQ(CollisionComponent->GetScaledBoundingBoxMax(), csp::common::Vector3(1, 1, 1));
	EXPECT_EQ(CollisionComponent->GetCollisionMode(), csp::multiplayer::CollisionMode::Trigger);
	EXPECT_EQ(CollisionComponent->GetCollisionShape(), csp::multiplayer::CollisionShape::Mesh);
	EXPECT_EQ(CollisionComponent->GetCollisionAssetId(), "TestAssetID");
	EXPECT_EQ(CollisionComponent->GetAssetCollectionId(), "TestAssetCollectionID");

	const float DefaultSphereRadius		 = CollisionSpaceComponent::GetDefaultSphereRadius();
	const float DefaultCapsuleHalfWidth	 = CollisionSpaceComponent::GetDefaultCapsuleHalfWidth();
	const float DefaultCapsuleHalfHeight = CollisionSpaceComponent::GetDefaultCapsuleHalfHeight();

	EXPECT_EQ(DefaultSphereRadius, 0.5f);
	EXPECT_EQ(DefaultCapsuleHalfWidth, 0.5f);
	EXPECT_EQ(DefaultCapsuleHalfHeight, 1.0f);

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_AUDIO_SCRIPT_INTERFACE_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, AudioScriptInterfaceTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	// Log in
	csp::common::String UserId;
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	// Create object to represent the audio
	csp::common::String ObjectName = "Object 1";
	SpaceTransform ObjectTransform = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};
	auto [CreatedObject]		   = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

	// Create audio component
	auto* AudioComponent = (AudioSpaceComponent*) CreatedObject->AddComponent(ComponentType::Audio);

	CreatedObject->QueueUpdate();
	EntitySystem->ProcessPendingEntityOperations();

	// Setup script
	std::string AudioScriptText = R"xx(
	
		const assetId			= "TEST_ASSET_ID";
		const assetCollectionId = "TEST_COLLECTION_ID";

		var audio = ThisEntity.getAudioComponents()[0];
		audio.position = [1,1,1];
		audio.playbackState = 2;
		audio.audioType = 1;
		audio.audioAssetId = assetId;
		audio.assetCollectionId = assetCollectionId;
		audio.attenuationRadius = 100;
		audio.isLoopPlayback = true;
		audio.timeSincePlay = 1;
		audio.volume = 0.75;
    )xx";

	CreatedObject->GetScript()->SetScriptSource(AudioScriptText.c_str());
	CreatedObject->GetScript()->Invoke();

	EntitySystem->ProcessPendingEntityOperations();

	// Ensure values are set correctly
	csp::common::String AssetId			  = "TEST_ASSET_ID";
	csp::common::String AssetCollectionId = "TEST_COLLECTION_ID";

	EXPECT_EQ(AudioComponent->GetPosition(), csp::common::Vector3::One());
	EXPECT_EQ(AudioComponent->GetPlaybackState(), AudioPlaybackState::Play);
	EXPECT_EQ(AudioComponent->GetAudioType(), AudioType::Spatial);
	EXPECT_EQ(AudioComponent->GetAudioAssetId(), AssetId);
	EXPECT_EQ(AudioComponent->GetAssetCollectionId(), AssetCollectionId);
	EXPECT_EQ(AudioComponent->GetAttenuationRadius(), 100.f);
	EXPECT_EQ(AudioComponent->GetIsLoopPlayback(), true);
	EXPECT_EQ(AudioComponent->GetTimeSincePlay(), 1.f);
	EXPECT_EQ(AudioComponent->GetVolume(), 0.75f);

	// Test invalid volume values
	AudioScriptText = R"xx(
		var audio = ThisEntity.getAudioComponents()[0];
		audio.volume = 1.75;
    )xx";
	CreatedObject->GetScript()->Invoke();
	EntitySystem->ProcessPendingEntityOperations();
	EXPECT_EQ(AudioComponent->GetVolume(), 0.75f);

	AudioScriptText = R"xx(M
		var audio = ThisEntity.getAudioComponents()[0];
		audio.volume = -2.75;
    )xx";
	CreatedObject->GetScript()->SetScriptSource(AudioScriptText.c_str());
	CreatedObject->GetScript()->Invoke();
	EntitySystem->ProcessPendingEntityOperations();
	EXPECT_EQ(AudioComponent->GetVolume(), 0.75f);

	// Test boundary volume values
	AudioScriptText = R"xx(
		var audio = ThisEntity.getAudioComponents()[0];
		audio.volume = 1.0;
    )xx";
	CreatedObject->GetScript()->SetScriptSource(AudioScriptText.c_str());
	CreatedObject->GetScript()->Invoke();
	EntitySystem->ProcessPendingEntityOperations();
	EXPECT_EQ(AudioComponent->GetVolume(), 1.f);

	AudioScriptText = R"xx(
		var audio = ThisEntity.getAudioComponents()[0];
		audio.volume = 0.0;
    )xx";
	CreatedObject->GetScript()->SetScriptSource(AudioScriptText.c_str());
	CreatedObject->GetScript()->Invoke();
	EntitySystem->ProcessPendingEntityOperations();
	EXPECT_EQ(AudioComponent->GetVolume(), 0.f);

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_USE_SPLINE_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, UseSplineTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	// Log in
	csp::common::String UserId;
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	const csp::common::String UserName		= "Player 1";
	const SpaceTransform UserTransform		= {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};
	const AvatarState UserAvatarState		= AvatarState::Idle;
	const csp::common::String UserAvatarId	= "MyCoolAvatar";
	const AvatarPlayMode UserAvatarPlayMode = AvatarPlayMode::Default;

	{

		auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
		EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
		Connection	 = EnterResult.GetConnection();
		EntitySystem = Connection->GetSpaceEntitySystem();

		EntitySystem->SetEntityCreatedCallback(
			[](SpaceEntity* Entity)
			{
			});

		// Ensure we're in the first space
		EXPECT_EQ(SpaceSystem->GetCurrentSpace().Id, Space.Id);

		// Create object to represent the spline
		csp::common::String ObjectName = "Object 1";
		SpaceTransform ObjectTransform = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};
		auto [CreatedObject]		   = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

		// Create spline component
		auto* SplineComponent							  = (SplineSpaceComponent*) CreatedObject->AddComponent(ComponentType::Spline);
		csp::common::List<csp::common::Vector3> WayPoints = {{0, 0, 0}, {0, 1000, 0}, {0, 2000, 0}, {0, 3000, 0}, {0, 4000, 0}, {0, 5000, 0}};

		{
			auto Result = SplineComponent->GetWaypoints();

			EXPECT_EQ(Result.Size(), 0);
		}

		{
			auto Result = SplineComponent->GetLocationAlongSpline(1);
			EXPECT_EQ(Result.X, 0);
			EXPECT_EQ(Result.Y, 0);
			EXPECT_EQ(Result.Z, 0);
		}

		{
			SplineComponent->SetWaypoints(WayPoints);

			auto Result = SplineComponent->GetWaypoints();

			EXPECT_EQ(Result.Size(), WayPoints.Size());

			// expect final waypoint to be the same
			EXPECT_EQ(Result[0], WayPoints[0]);
		}

		{
			// Calculated cubic interpolate spline
			auto Result = SplineComponent->GetLocationAlongSpline(1);

			// expect final waypoint to be the same
			EXPECT_EQ(Result, WayPoints[WayPoints.Size() - 1]);
		}

		AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);
	}

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_SPLINE_SCRIPT_INTERFACE_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, SplineScriptInterfaceTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	// Log in
	csp::common::String UserId;
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	// Create object to represent the spline
	csp::common::String ObjectName = "Object 1";
	SpaceTransform ObjectTransform = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};
	auto [CreatedObject]		   = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

	// Create spline component
	auto* SplineComponent							  = (SplineSpaceComponent*) CreatedObject->AddComponent(ComponentType::Spline);
	csp::common::List<csp::common::Vector3> WayPoints = {{0, 0, 0}, {0, 1000, 0}, {0, 2000, 0}, {0, 3000, 0}, {0, 4000, 0}, {0, 5000, 0}};

	CreatedObject->QueueUpdate();
	EntitySystem->ProcessPendingEntityOperations();

	// Setup script
	const std::string SplineScriptText = R"xx(
	
		var spline = ThisEntity.getSplineComponents()[0];
		
		var waypoints = [[0, 0, 0], [0, 1000, 0], [0, 2000, 0], [0, 3000, 0], [0, 4000, 0], [0, 5000, 0]];
		spline.setWaypoints(waypoints);
		var positionResult = spline.getLocationAlongSpline(1);
		
    )xx";

	CreatedObject->GetScript()->SetScriptSource(SplineScriptText.c_str());
	CreatedObject->GetScript()->Invoke();

	EntitySystem->ProcessPendingEntityOperations();

	EXPECT_EQ(SplineComponent->GetWaypoints().Size(), WayPoints.Size());

	// expect final waypoint to be the same
	EXPECT_EQ(SplineComponent->GetWaypoints()[0], WayPoints[0]);

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_SCRIPT_DELTA_TIME_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, ScriptDeltaTimeTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	const std::string ScriptText = R"xx(

        var entities = TheEntitySystem.getEntities();
		var entityIndex = TheEntitySystem.getIndexOfEntity(ThisEntity.id);
		
		globalThis.onTick = (_evtName, params) => {
			 const { deltaTimeMS } = JSON.parse(params);
			 CSP.Log(_evtName);
			 CSP.Log(deltaTimeMS);
		}

		ThisEntity.subscribeToMessage("entityTick", "onTick");
		  
    )xx";

	{
		const csp::common::String ObjectName = "Object 1";
		SpaceTransform ObjectTransform		 = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};
		auto [Object]						 = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

		ScriptSpaceComponent* ScriptComponent = static_cast<ScriptSpaceComponent*>(Object->AddComponent(ComponentType::ScriptData));

		Object->QueueUpdate();
		EntitySystem->ProcessPendingEntityOperations();

		ScriptComponent->SetScriptSource(csp::common::String(ScriptText.c_str()));
		Object->GetScript()->Invoke();

		csp::CSPFoundation::Tick();

		const bool ScriptHasErrors = Object->GetScript()->HasError();
		EXPECT_FALSE(ScriptHasErrors);
	}

	// Delete MultiplayerConnection
	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}

#endif


#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_LIGHT_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, LightComponentFieldsTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();
	auto* AssetSystem	 = SystemsManager.GetAssetSystem();

	const char* TestSpaceName			= "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription	= "OLY-UNITTEST-SPACEDESC-REWIND";
	const char* TestAssetCollectionName = "OLY-UNITTEST-ASSETCOLLECTION-REWIND";
	const char* TestAssetName			= "OLY-UNITTEST-ASSET-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	bool AssetDetailBlobChangedCallbackCalled = false;
	csp::common::String CallbackAssetId;

	const csp::common::String ObjectName = "Object 1";
	SpaceTransform ObjectTransform		 = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};

	auto [Object] = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

	const csp::common::String ModelAssetId = "NotARealId";

	auto* LightSpaceComponentInstance = (LightSpaceComponent*) Object->AddComponent(ComponentType::Light);

	// Process component creation
	Object->QueueUpdate();
	EntitySystem->ProcessPendingEntityOperations();

	// Check component was created
	auto& Components = *Object->GetComponents();
	ASSERT_EQ(Components.Size(), 1);

	char UniqueAssetCollectionName[256];
	SPRINTF(UniqueAssetCollectionName, "%s-%s", TestAssetCollectionName, GetUniqueHexString().c_str());

	char UniqueAssetName[256];
	SPRINTF(UniqueAssetName, "%s-%s", TestAssetName, GetUniqueHexString().c_str());

	// Create asset collection
	csp::systems::AssetCollection AssetCollection;
	CreateAssetCollection(AssetSystem, Space.Id, nullptr, UniqueAssetCollectionName, nullptr, nullptr, AssetCollection);

	// Create asset
	csp::systems::Asset Asset;
	CreateAsset(AssetSystem, AssetCollection, UniqueAssetName, nullptr, nullptr, Asset);
	Asset.FileName = "OKO.png";
	Asset.Name	   = "OKO";
	Asset.Type	   = csp::systems::EAssetType::IMAGE;

	auto UploadFilePath		 = std::filesystem::absolute("assets/OKO.png");
	FILE* UploadFile		 = fopen(UploadFilePath.string().c_str(), "rb");
	uintmax_t UploadFileSize = std::filesystem::file_size(UploadFilePath);
	auto* UploadFileData	 = new unsigned char[UploadFileSize];
	fread(UploadFileData, UploadFileSize, 1, UploadFile);
	fclose(UploadFile);

	csp::systems::BufferAssetDataSource BufferSource;
	BufferSource.Buffer		  = UploadFileData;
	BufferSource.BufferLength = UploadFileSize;

	BufferSource.SetMimeType("image/png");

	printf("Uploading asset data...\n");

	// Upload data
	UploadAssetData(AssetSystem, AssetCollection, Asset, BufferSource, Asset.Uri);

	delete[] UploadFileData;

	EXPECT_EQ(LightSpaceComponentInstance->GetLightCookieType(), LightCookieType::NoCookie);
	EXPECT_EQ(LightSpaceComponentInstance->GetLightType(), LightType::Point);

	// test values
	const float InnerConeAngle = 10.0f;
	const float OuterConeAngle = 20.0f;
	const float Range		   = 120.0f;
	const float Intensity	   = 1000.0f;

	LightSpaceComponentInstance->SetLightCookieType(LightCookieType::ImageCookie);
	LightSpaceComponentInstance->SetLightCookieAssetCollectionId(Asset.AssetCollectionId);
	LightSpaceComponentInstance->SetLightCookieAssetId(Asset.Id);
	LightSpaceComponentInstance->SetLightType(LightType::Spot);
	LightSpaceComponentInstance->SetInnerConeAngle(InnerConeAngle);
	LightSpaceComponentInstance->SetOuterConeAngle(OuterConeAngle);
	LightSpaceComponentInstance->SetRange(Range);
	LightSpaceComponentInstance->SetIntensity(Intensity);

	auto LightSpaceComponentKey				= LightSpaceComponentInstance->GetId();
	auto* StoredLightSpaceComponentInstance = (LightSpaceComponent*) Object->GetComponent(LightSpaceComponentKey);

	EXPECT_EQ(StoredLightSpaceComponentInstance->GetLightCookieType(), LightCookieType::ImageCookie);
	EXPECT_EQ(StoredLightSpaceComponentInstance->GetLightCookieAssetCollectionId(), Asset.AssetCollectionId);
	EXPECT_EQ(StoredLightSpaceComponentInstance->GetLightCookieAssetId(), Asset.Id);
	EXPECT_EQ(StoredLightSpaceComponentInstance->GetLightType(), LightType::Spot);
	EXPECT_EQ(StoredLightSpaceComponentInstance->GetInnerConeAngle(), InnerConeAngle);
	EXPECT_EQ(StoredLightSpaceComponentInstance->GetOuterConeAngle(), OuterConeAngle);
	EXPECT_EQ(StoredLightSpaceComponentInstance->GetRange(), Range);
	EXPECT_EQ(StoredLightSpaceComponentInstance->GetIntensity(), Intensity);

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete asset
	DeleteAsset(AssetSystem, AssetCollection, Asset);

	// Delete asset collection
	DeleteAssetCollection(AssetSystem, AssetCollection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_CUSTOM_COMPONENT_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, CustomComponentTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	const char* TestSpaceName2		  = "OLY-UNITTEST-SPACE-REWIND-2";
	const char* TestSpaceDescription2 = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	char UniqueSpaceName2[256];
	SPRINTF(UniqueSpaceName2, "%s-%s", TestSpaceName2, GetUniqueHexString().c_str());

	// Log in
	csp::common::String UserId;
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	{

		auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
		EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
		Connection	 = EnterResult.GetConnection();
		EntitySystem = Connection->GetSpaceEntitySystem();

		EntitySystem->SetEntityCreatedCallback(
			[](SpaceEntity* Entity)
			{
			});

		// Create object to represent the Custom fields
		csp::common::String ObjectName = "Object 1";
		SpaceTransform ObjectTransform = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};
		auto [CreatedObject]		   = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

		// Create custom component
		auto* CustomComponent = (CustomSpaceComponent*) CreatedObject->AddComponent(ComponentType::Custom);

		EXPECT_EQ(CustomComponent->GetCustomPropertyKeys().Size(), 0);

		// Vector Check
		{
			CustomComponent->SetCustomProperty("Vector3", ReplicatedValue({10, 10, 10}));
			EXPECT_EQ(CustomComponent->GetCustomProperty("Vector3").GetVector3(), csp::common::Vector3({10, 10, 10}));

			CustomComponent->SetCustomProperty("Vector4", ReplicatedValue({10, 10, 10, 10}));
			EXPECT_EQ(CustomComponent->GetCustomProperty("Vector4").GetVector4(), csp::common::Vector4({10, 10, 10, 10}));
		}

		// String Check
		{
			CustomComponent->SetCustomProperty("String", ReplicatedValue("OKO"));
			EXPECT_EQ(CustomComponent->GetCustomProperty("String").GetString(), "OKO");
		}

		// Boolean Check
		{
			CustomComponent->SetCustomProperty("Boolean", ReplicatedValue(true));
			EXPECT_EQ(CustomComponent->GetCustomProperty("Boolean").GetBool(), true);
		}

		// Integer Check
		{
			CustomComponent->SetCustomProperty("Integer", ReplicatedValue(int64_t(1)));
			EXPECT_EQ(CustomComponent->GetCustomProperty("Integer").GetInt(), int64_t(1));
		}

		// Float Check
		{
			CustomComponent->SetCustomProperty("Float", ReplicatedValue(1.00f));
			EXPECT_EQ(CustomComponent->GetCustomProperty("Float").GetFloat(), 1.00f);
		}

		// Has Key Check
		{
			EXPECT_EQ(CustomComponent->HasCustomProperty("Boolean"), true);
			EXPECT_EQ(CustomComponent->HasCustomProperty("BooleanFalse"), false);
		}

		// Key Size
		{
			EXPECT_EQ(CustomComponent->GetNumProperties(), 6);
		}

		// Remove Key
		{
			CustomComponent->RemoveCustomProperty("Boolean");
			EXPECT_EQ(CustomComponent->GetNumProperties(), 5);
		}

		// List Check
		{
			auto Keys = CustomComponent->GetCustomPropertyKeys();
			EXPECT_EQ(Keys.Size(), 5);
		}

		AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);
	}

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_CUSTOM_COMPONENT_SCRIPT_INTERFACE_SUBSCRIPTION_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, CustomComponentScriptInterfaceSubscriptionTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	// Log in
	csp::common::String UserId;
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	std::atomic_bool ScriptSystemReady = false;

	auto EntityCreatedCallback = [](SpaceEntity* Entity)
	{
		std::cerr << "EntityCreatedCallback called" << std::endl;
	};

	auto EntitiesReadyCallback = [](bool Ok)
	{
		EXPECT_EQ(Ok, true);
		std::cerr << "EntitiesReadyCallback called" << std::endl;
	};

	auto ScriptSystemReadyCallback = [&ScriptSystemReady](bool Ok)
	{
		EXPECT_EQ(Ok, true);
		std::cerr << "ScriptSystemReadyCallback called" << std::endl;
		ScriptSystemReady = true;
	};

	SpaceSystem->SetEntityCreatedCallback(EntityCreatedCallback);
	SpaceSystem->SetInitialEntitiesRetrievedCallback(EntitiesReadyCallback);
	SpaceSystem->SetScriptSystemReadyCallback(ScriptSystemReadyCallback);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();
	OnConnect();

	// Create object to represent the audio
	csp::common::String ObjectName = "Object 1";
	SpaceTransform ObjectTransform = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};
	auto [CreatedObject]		   = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

	// Create audio component
	auto* CustomComponent = (CustomSpaceComponent*) CreatedObject->AddComponent(ComponentType::Custom);

	CustomComponent->SetCustomProperty("Number", ReplicatedValue(int64_t(0)));
	CustomComponent->SetCustomProperty("NumberChanged", ReplicatedValue(false));

	CreatedObject->QueueUpdate();
	EntitySystem->ProcessPendingEntityOperations();

	// Setup script
	std::string ScriptText = R"xx(
		var custom = ThisEntity.getCustomComponents()[0];
		custom.setCustomProperty("testFloat", 1.234);
		custom.setCustomProperty("testInt", 1234);
		globalThis.onValueChanged = () => {
		  custom.setCustomProperty("NumberChanged", true);
		}  
		// subscribe to entity events 
		ThisEntity.subscribeToPropertyChange(custom.id, custom.getCustomPropertySubscriptionKey("Number"), "valueChanged");
		ThisEntity.subscribeToMessage("valueChanged", "onValueChanged");
		)xx";

	auto ScriptSystemIsReady = [&ScriptSystemReady]()
	{
		std::cerr << "Waiting for ScriptSystemReady" << std::endl;
		return (ScriptSystemReady == true);
	};

	EXPECT_EQ(ResponseWaiter::WaitFor(ScriptSystemIsReady, std::chrono::seconds(5)), true);

	CreatedObject->GetScript()->SetScriptSource(ScriptText.c_str());
	CreatedObject->GetScript()->Invoke();

	EntitySystem->ProcessPendingEntityOperations();
	EXPECT_EQ(CustomComponent->GetCustomProperty("testFloat").GetFloat(), 1.234f);
	EXPECT_EQ(CustomComponent->GetCustomProperty("testInt").GetInt(), 1234);
	EXPECT_EQ(CustomComponent->GetCustomProperty("Number").GetInt(), 0);
	EXPECT_FALSE(CustomComponent->GetCustomProperty("NumberChanged").GetBool());

	CustomComponent->SetCustomProperty("Number", ReplicatedValue(int64_t(100)));

	EXPECT_EQ(CustomComponent->GetCustomProperty("Number").GetInt(), 100);
	EXPECT_TRUE(CustomComponent->GetCustomProperty("NumberChanged").GetBool());

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_EXTERNAL_LINK_COMPONENT_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, ExternalLinkComponentTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	const char* TestSpaceName2		  = "OLY-UNITTEST-SPACE-REWIND-2";
	const char* TestSpaceDescription2 = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	// Log in
	csp::common::String UserId;
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	{

		auto* Connection   = new csp::multiplayer::MultiplayerConnection(Space.Id);
		auto* EntitySystem = Connection->GetSpaceEntitySystem();

		EntitySystem->SetEntityCreatedCallback(
			[](SpaceEntity* Entity)
			{
			});

		// Connect to the SignalR server
		auto [Ok] = AWAIT(Connection, Connect);

		EXPECT_TRUE(Ok);

		std::tie(Ok) = AWAIT(Connection, InitialiseConnection);

		EXPECT_TRUE(Ok);

		csp::common::String ObjectName = "Object 1";
		SpaceTransform ObjectTransform = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};
		auto [CreatedObject]		   = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

		// Create custom component
		auto* ExternalLinkComponent = (ExternalLinkSpaceComponent*) CreatedObject->AddComponent(ComponentType::ExternalLink);

		const csp::common::String ExternalLinkName = "MyExternalLink";
		ExternalLinkComponent->SetName(ExternalLinkName);
		EXPECT_EQ(ExternalLinkComponent->GetName(), ExternalLinkName);

		const csp::common::String ExternalLinkUrl = "https://oko.live";
		ExternalLinkComponent->SetLinkUrl(ExternalLinkUrl);
		EXPECT_EQ(ExternalLinkComponent->GetLinkUrl(), ExternalLinkUrl);

		const csp::common::Vector3 Position(123.0f, 456.0f, 789.0f);
		ExternalLinkComponent->SetPosition(Position);
		EXPECT_EQ(ExternalLinkComponent->GetPosition(), Position);

		const csp::common::Vector4 Rotation(1.0f, 2.0f, 3.0f, 4.0f);
		ExternalLinkComponent->SetRotation(Rotation);
		EXPECT_EQ(ExternalLinkComponent->GetRotation(), Rotation);

		const csp::common::Vector3 Scale(123.0f, 456.0f, 789.0f);
		ExternalLinkComponent->SetScale(Scale);
		EXPECT_EQ(ExternalLinkComponent->GetScale(), Scale);

		const csp::common::String DisplayText = "A great link";
		ExternalLinkComponent->SetDisplayText(DisplayText);
		EXPECT_EQ(ExternalLinkComponent->GetDisplayText(), DisplayText);

		bool IsEnabled = false;
		ExternalLinkComponent->SetIsEnabled(IsEnabled);
		EXPECT_EQ(ExternalLinkComponent->GetIsEnabled(), IsEnabled);

		bool IsVisible = false;
		ExternalLinkComponent->SetIsVisible(IsVisible);
		EXPECT_EQ(ExternalLinkComponent->GetIsVisible(), IsVisible);

		bool IsARVisible = false;
		ExternalLinkComponent->SetIsARVisible(IsARVisible);
		EXPECT_EQ(ExternalLinkComponent->GetIsARVisible(), IsARVisible);

		// Disconnect from the SignalR server
		std::tie(Ok) = AWAIT(Connection, Disconnect);

		EXPECT_TRUE(Ok);

		// Delete MultiplayerConnection
		delete Connection;
	}

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_ACTIONHANDLER_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, ActionHandlerTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();
	auto* AssetSystem	 = SystemsManager.GetAssetSystem();

	const char* TestSpaceName			= "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription	= "OLY-UNITTEST-SPACEDESC-REWIND";
	const char* TestAssetCollectionName = "OLY-UNITTEST-ASSETCOLLECTION-REWIND";
	const char* TestAssetName			= "OLY-UNITTEST-ASSET-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	bool AssetDetailBlobChangedCallbackCalled = false;
	csp::common::String CallbackAssetId;

	const csp::common::String ObjectName = "Object 1";
	SpaceTransform ObjectTransform		 = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};

	auto [Object] = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

	const csp::common::String ModelAssetId = "NotARealId";

	auto* LightSpaceComponentInstance = (LightSpaceComponent*) Object->AddComponent(ComponentType::Light);

	// Process component creation
	Object->QueueUpdate();
	EntitySystem->ProcessPendingEntityOperations();

	// Check component was created
	auto& Components = *Object->GetComponents();
	ASSERT_EQ(Components.Size(), 1);

	bool ActionCalled = false;
	LightSpaceComponentInstance->RegisterActionHandler("TestAction",
													   [&ActionCalled](ComponentBase*, const csp::common::String, const csp::common::String)
													   {
														   ActionCalled = true;
													   });

	LightSpaceComponentInstance->InvokeAction("TestAction", "TestParam");

	ASSERT_TRUE(ActionCalled);

	// Disconnect from the SignalR server
	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_LIGHT_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, LightComponentTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();
	auto* AssetSystem	 = SystemsManager.GetAssetSystem();

	const char* TestSpaceName			= "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription	= "OLY-UNITTEST-SPACEDESC-REWIND";
	const char* TestAssetCollectionName = "OLY-UNITTEST-ASSETCOLLECTION-REWIND";
	const char* TestAssetName			= "OLY-UNITTEST-ASSET-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto* Connection   = new csp::multiplayer::MultiplayerConnection(Space.Id);
	auto* EntitySystem = Connection->GetSpaceEntitySystem();

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	// Connect to the SignalR server
	auto [Ok] = AWAIT(Connection, Connect);

	EXPECT_TRUE(Ok);

	std::tie(Ok) = AWAIT(Connection, InitialiseConnection);

	EXPECT_TRUE(Ok);

	bool AssetDetailBlobChangedCallbackCalled = false;
	csp::common::String CallbackAssetId;

	const csp::common::String ObjectName = "Object 1";
	SpaceTransform ObjectTransform		 = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};

	auto [Object] = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

	const csp::common::String ModelAssetId = "NotARealId";

	auto* LightSpaceComponentInstance = (LightSpaceComponent*) Object->AddComponent(ComponentType::Light);

	// Process component creation
	Object->QueueUpdate();
	EntitySystem->ProcessPendingEntityOperations();

	// Check component was created
	auto& Components = *Object->GetComponents();
	ASSERT_EQ(Components.Size(), 1);

	char UniqueAssetCollectionName[256];
	SPRINTF(UniqueAssetCollectionName, "%s-%s", TestAssetCollectionName, GetUniqueHexString().c_str());

	char UniqueAssetName[256];
	SPRINTF(UniqueAssetName, "%s-%s", TestAssetName, GetUniqueHexString().c_str());

	// Create asset collection
	csp::systems::AssetCollection AssetCollection;
	CreateAssetCollection(AssetSystem, Space.Id, nullptr, UniqueAssetCollectionName, nullptr, nullptr, AssetCollection);

	// Create asset
	csp::systems::Asset Asset;
	CreateAsset(AssetSystem, AssetCollection, UniqueAssetName, nullptr, nullptr, Asset);
	Asset.FileName = "OKO.png";
	Asset.Name	   = "OKO";
	Asset.Type	   = csp::systems::EAssetType::IMAGE;

	auto UploadFilePath		 = std::filesystem::absolute("assets/OKO.png");
	FILE* UploadFile		 = fopen(UploadFilePath.string().c_str(), "rb");
	uintmax_t UploadFileSize = std::filesystem::file_size(UploadFilePath);
	auto* UploadFileData	 = new unsigned char[UploadFileSize];
	fread(UploadFileData, UploadFileSize, 1, UploadFile);
	fclose(UploadFile);

	csp::systems::BufferAssetDataSource BufferSource;
	BufferSource.Buffer		  = UploadFileData;
	BufferSource.BufferLength = UploadFileSize;

	BufferSource.SetMimeType("image/png");

	printf("Uploading asset data...\n");

	// Upload data
	UploadAssetData(AssetSystem, AssetCollection, Asset, BufferSource, Asset.Uri);

	delete[] UploadFileData;

	EXPECT_EQ(LightSpaceComponentInstance->GetLightCookieType(), LightCookieType::NoCookie);
	EXPECT_EQ(LightSpaceComponentInstance->GetLightType(), LightType::Point);
	EXPECT_EQ(LightSpaceComponentInstance->GetInnerConeAngle(), 0.0f);
	EXPECT_EQ(LightSpaceComponentInstance->GetOuterConeAngle(), 0.78539816339f);
	EXPECT_EQ(LightSpaceComponentInstance->GetRange(), 1000.0f);
	EXPECT_EQ(LightSpaceComponentInstance->GetIntensity(), 5000.0f);

	// test values
	const float InnerConeAngle = 10.0f;
	const float OuterConeAngle = 20.0f;
	const float Range		   = 120.0f;
	const float Intensity	   = 1000.0f;

	LightSpaceComponentInstance->SetLightCookieAssetCollectionId(Asset.AssetCollectionId);
	LightSpaceComponentInstance->SetLightCookieAssetId(Asset.Id);
	LightSpaceComponentInstance->SetLightCookieType(LightCookieType::ImageCookie);
	LightSpaceComponentInstance->SetLightType(LightType::Spot);
	LightSpaceComponentInstance->SetInnerConeAngle(InnerConeAngle);
	LightSpaceComponentInstance->SetOuterConeAngle(OuterConeAngle);
	LightSpaceComponentInstance->SetRange(Range);
	LightSpaceComponentInstance->SetIntensity(Intensity);

	auto LightSpaceComponentKey				= LightSpaceComponentInstance->GetId();
	auto* StoredLightSpaceComponentInstance = (LightSpaceComponent*) Object->GetComponent(LightSpaceComponentKey);

	EXPECT_EQ(StoredLightSpaceComponentInstance->GetLightCookieType(), LightCookieType::ImageCookie);
	EXPECT_EQ(StoredLightSpaceComponentInstance->GetLightCookieAssetCollectionId(), Asset.AssetCollectionId);
	EXPECT_EQ(StoredLightSpaceComponentInstance->GetLightCookieAssetId(), Asset.Id);
	EXPECT_EQ(StoredLightSpaceComponentInstance->GetLightType(), LightType::Spot);
	EXPECT_EQ(StoredLightSpaceComponentInstance->GetInnerConeAngle(), InnerConeAngle);
	EXPECT_EQ(StoredLightSpaceComponentInstance->GetOuterConeAngle(), OuterConeAngle);
	EXPECT_EQ(StoredLightSpaceComponentInstance->GetRange(), Range);
	EXPECT_EQ(StoredLightSpaceComponentInstance->GetIntensity(), Intensity);

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_IMAGE_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, ImageComponentTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();
	auto* AssetSystem	 = SystemsManager.GetAssetSystem();

	const char* TestSpaceName			= "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription	= "OLY-UNITTEST-SPACEDESC-REWIND";
	const char* TestAssetCollectionName = "OLY-UNITTEST-ASSETCOLLECTION-REWIND";
	const char* TestAssetName			= "OLY-UNITTEST-ASSET-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto* Connection   = new csp::multiplayer::MultiplayerConnection(Space.Id);
	auto* EntitySystem = Connection->GetSpaceEntitySystem();

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	// Connect to the SignalR server
	auto [Ok] = AWAIT(Connection, Connect);

	EXPECT_TRUE(Ok);

	std::tie(Ok) = AWAIT(Connection, InitialiseConnection);

	EXPECT_TRUE(Ok);

	bool AssetDetailBlobChangedCallbackCalled = false;
	csp::common::String CallbackAssetId;

	const csp::common::String ObjectName = "Object 1";
	SpaceTransform ObjectTransform		 = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};

	auto [Object] = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

	const csp::common::String ModelAssetId = "NotARealId";

	auto* ImageSpaceComponentInstance = (ImageSpaceComponent*) Object->AddComponent(ComponentType::Image);

	// Process component creation
	Object->QueueUpdate();
	EntitySystem->ProcessPendingEntityOperations();

	// Check component was created
	auto& Components = *Object->GetComponents();
	ASSERT_EQ(Components.Size(), 1);

	char UniqueAssetCollectionName[256];
	SPRINTF(UniqueAssetCollectionName, "%s-%s", TestAssetCollectionName, GetUniqueHexString().c_str());

	char UniqueAssetName[256];
	SPRINTF(UniqueAssetName, "%s-%s", TestAssetName, GetUniqueHexString().c_str());

	// Create asset collection
	csp::systems::AssetCollection AssetCollection;
	CreateAssetCollection(AssetSystem, Space.Id, nullptr, UniqueAssetCollectionName, nullptr, nullptr, AssetCollection);

	// Create asset
	csp::systems::Asset Asset;
	CreateAsset(AssetSystem, AssetCollection, UniqueAssetName, nullptr, nullptr, Asset);
	Asset.FileName = "OKO.png";
	Asset.Name	   = "OKO";
	Asset.Type	   = csp::systems::EAssetType::IMAGE;

	auto UploadFilePath		 = std::filesystem::absolute("assets/OKO.png");
	FILE* UploadFile		 = fopen(UploadFilePath.string().c_str(), "rb");
	uintmax_t UploadFileSize = std::filesystem::file_size(UploadFilePath);
	auto* UploadFileData	 = new unsigned char[UploadFileSize];
	fread(UploadFileData, UploadFileSize, 1, UploadFile);
	fclose(UploadFile);

	csp::systems::BufferAssetDataSource BufferSource;
	BufferSource.Buffer		  = UploadFileData;
	BufferSource.BufferLength = UploadFileSize;

	BufferSource.SetMimeType("image/png");

	printf("Uploading asset data...\n");

	// Upload data
	UploadAssetData(AssetSystem, AssetCollection, Asset, BufferSource, Asset.Uri);

	delete[] UploadFileData;

	EXPECT_EQ(ImageSpaceComponentInstance->GetBillboardMode(), BillboardMode::Off);
	EXPECT_EQ(ImageSpaceComponentInstance->GetDisplayMode(), DisplayMode::DoubleSided);
	EXPECT_EQ(ImageSpaceComponentInstance->GetIsARVisible(), true);
	EXPECT_EQ(ImageSpaceComponentInstance->GetIsEmissive(), false);


	ImageSpaceComponentInstance->SetAssetCollectionId(Asset.AssetCollectionId);
	ImageSpaceComponentInstance->SetImageAssetId(Asset.Id);
	ImageSpaceComponentInstance->SetBillboardMode(BillboardMode::YawLockedBillboard);
	ImageSpaceComponentInstance->SetDisplayMode(DisplayMode::SingleSided);
	ImageSpaceComponentInstance->SetIsARVisible(false);
	ImageSpaceComponentInstance->SetIsEmissive(true);

	auto ImageSpaceComponentKey		= ImageSpaceComponentInstance->GetId();
	auto* StoredImageSpaceComponent = (ImageSpaceComponent*) Object->GetComponent(ImageSpaceComponentKey);

	EXPECT_EQ(StoredImageSpaceComponent->GetAssetCollectionId(), Asset.AssetCollectionId);
	EXPECT_EQ(StoredImageSpaceComponent->GetImageAssetId(), Asset.Id);
	EXPECT_EQ(StoredImageSpaceComponent->GetBillboardMode(), BillboardMode::YawLockedBillboard);
	EXPECT_EQ(StoredImageSpaceComponent->GetDisplayMode(), DisplayMode::SingleSided);
	EXPECT_EQ(StoredImageSpaceComponent->GetIsARVisible(), false);
	EXPECT_EQ(StoredImageSpaceComponent->GetIsEmissive(), true);

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_REFLECTION_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, ReflectionComponentTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();
	auto* AssetSystem	 = SystemsManager.GetAssetSystem();

	const char* TestSpaceName			= "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription	= "OLY-UNITTEST-SPACEDESC-REWIND";
	const char* TestAssetCollectionName = "OLY-UNITTEST-ASSETCOLLECTION-REWIND";
	const char* TestAssetName			= "OLY-UNITTEST-ASSET-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto* Connection   = new csp::multiplayer::MultiplayerConnection(Space.Id);
	auto* EntitySystem = Connection->GetSpaceEntitySystem();

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	// Connect to the SignalR server
	auto [Ok] = AWAIT(Connection, Connect);

	EXPECT_TRUE(Ok);

	std::tie(Ok) = AWAIT(Connection, InitialiseConnection);

	EXPECT_TRUE(Ok);

	bool AssetDetailBlobChangedCallbackCalled = false;
	csp::common::String CallbackAssetId;

	const csp::common::String ObjectName = "Object 1";
	SpaceTransform ObjectTransform		 = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};

	auto [Object] = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

	const csp::common::String ModelAssetId = "NotARealId";

	auto* ReflectionSpaceComponentInstance = (ReflectionSpaceComponent*) Object->AddComponent(ComponentType::Reflection);

	// Process component creation
	Object->QueueUpdate();
	EntitySystem->ProcessPendingEntityOperations();

	// Check component was created
	auto& Components = *Object->GetComponents();
	ASSERT_EQ(Components.Size(), 1);

	char UniqueAssetCollectionName[256];
	SPRINTF(UniqueAssetCollectionName, "%s-%s", TestAssetCollectionName, GetUniqueHexString().c_str());

	char UniqueAssetName[256];
	SPRINTF(UniqueAssetName, "%s-%s", TestAssetName, GetUniqueHexString().c_str());

	// Create asset collection
	csp::systems::AssetCollection AssetCollection;
	CreateAssetCollection(AssetSystem, Space.Id, nullptr, UniqueAssetCollectionName, nullptr, nullptr, AssetCollection);

	// Create asset
	csp::systems::Asset Asset;
	CreateAsset(AssetSystem, AssetCollection, UniqueAssetName, nullptr, nullptr, Asset);
	Asset.FileName = "OKO.png";
	Asset.Name	   = "OKO";
	Asset.Type	   = csp::systems::EAssetType::IMAGE;

	auto UploadFilePath		 = std::filesystem::absolute("assets/OKO.png");
	FILE* UploadFile		 = fopen(UploadFilePath.string().c_str(), "rb");
	uintmax_t UploadFileSize = std::filesystem::file_size(UploadFilePath);
	auto* UploadFileData	 = new unsigned char[UploadFileSize];
	fread(UploadFileData, UploadFileSize, 1, UploadFile);
	fclose(UploadFile);

	csp::systems::BufferAssetDataSource BufferSource;
	BufferSource.Buffer		  = UploadFileData;
	BufferSource.BufferLength = UploadFileSize;

	BufferSource.SetMimeType("image/png");

	printf("Uploading asset data...\n");

	// Upload data
	UploadAssetData(AssetSystem, AssetCollection, Asset, BufferSource, Asset.Uri);

	delete[] UploadFileData;

	EXPECT_EQ(ReflectionSpaceComponentInstance->GetReflectionShape(), ReflectionShape::UnitBox);

	ReflectionSpaceComponentInstance->SetAssetCollectionId(Asset.AssetCollectionId);
	ReflectionSpaceComponentInstance->SetReflectionAssetId(Asset.Id);
	ReflectionSpaceComponentInstance->SetReflectionShape(ReflectionShape::UnitSphere);

	auto ReflectionSpaceComponentKey	 = ReflectionSpaceComponentInstance->GetId();
	auto* StoredReflectionSpaceComponent = (ReflectionSpaceComponent*) Object->GetComponent(ReflectionSpaceComponentKey);

	EXPECT_EQ(StoredReflectionSpaceComponent->GetAssetCollectionId(), Asset.AssetCollectionId);
	EXPECT_EQ(StoredReflectionSpaceComponent->GetReflectionAssetId(), Asset.Id);
	EXPECT_EQ(StoredReflectionSpaceComponent->GetReflectionShape(), ReflectionShape::UnitSphere);

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_IMAGE_SCRIPT_INTERFACE_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, ImageScriptInterfaceTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	// Log in
	csp::common::String UserId;
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);

	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);

	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	// Create object to represent the image
	csp::common::String ObjectName = "Object 1";
	SpaceTransform ObjectTransform = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};
	auto [CreatedObject]		   = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

	// Create image component
	auto* ImageComponent = (ImageSpaceComponent*) CreatedObject->AddComponent(ComponentType::Image);
	// Create script component
	auto* ScriptComponent = (ScriptSpaceComponent*) CreatedObject->AddComponent(ComponentType::ScriptData);

	CreatedObject->QueueUpdate();
	EntitySystem->ProcessPendingEntityOperations();

	EXPECT_EQ(ImageComponent->GetIsVisible(), true);
	EXPECT_EQ(ImageComponent->GetIsEmissive(), false);
	EXPECT_EQ(ImageComponent->GetDisplayMode(), DisplayMode::DoubleSided);
	EXPECT_EQ(ImageComponent->GetBillboardMode(), BillboardMode::Off);

	// Setup script
	const std::string ImageScriptText = R"xx(
	
		var image = ThisEntity.getImageComponents()[0];
		
		image.isVisible = false;
		image.isEmissive = true;
		image.displayMode = 2;
		image.billboardMode = 1;
    )xx";

	ScriptComponent->SetScriptSource(ImageScriptText.c_str());
	CreatedObject->GetScript()->Invoke();

	EntitySystem->ProcessPendingEntityOperations();

	const bool ScriptHasErrors = CreatedObject->GetScript()->HasError();
	EXPECT_FALSE(ScriptHasErrors);

	EXPECT_EQ(ImageComponent->GetIsVisible(), false);
	EXPECT_EQ(ImageComponent->GetIsEmissive(), true);
	EXPECT_EQ(ImageComponent->GetDisplayMode(), DisplayMode::DoubleSidedReversed);
	EXPECT_EQ(ImageComponent->GetBillboardMode(), BillboardMode::Billboard);

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

// Derived type that allows us to access protected members of SpaceEntitySystem
struct InternalSpaceEntitySystem : public csp::multiplayer::SpaceEntitySystem
{
	void ClearEntities()
	{
		std::scoped_lock<std::recursive_mutex> EntitiesLocker(*EntitiesLock);

		Entities.Clear();
	}
};

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_FOG_COMPONENT_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, FogComponentTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	// Log in
	csp::common::String UserId;
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	// Create object to represent the fog
	csp::common::String ObjectName = "Object 1";
	SpaceTransform ObjectTransform = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};
	auto [CreatedObject]		   = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

	// Create fog component
	auto* FogComponent = static_cast<FogSpaceComponent*>(CreatedObject->AddComponent(ComponentType::Fog));

	// Ensure defaults are set
	EXPECT_EQ(FogComponent->GetFogMode(), FogMode::Linear);
	EXPECT_EQ(FogComponent->GetPosition(), csp::common::Vector3::Zero());
	EXPECT_EQ(FogComponent->GetRotation(), csp::common::Vector4(0, 0, 0, 1));
	EXPECT_EQ(FogComponent->GetScale(), csp::common::Vector3::One());
	EXPECT_FLOAT_EQ(FogComponent->GetStartDistance(), 0.f);
	EXPECT_FLOAT_EQ(FogComponent->GetEndDistance(), 0.f);
	EXPECT_EQ(FogComponent->GetColor(), csp::common::Vector3({0.8f, 0.9f, 1.0f}));
	EXPECT_FLOAT_EQ(FogComponent->GetDensity(), 0.2f);
	EXPECT_FLOAT_EQ(FogComponent->GetHeightFalloff(), 0.2f);
	EXPECT_FLOAT_EQ(FogComponent->GetMaxOpacity(), 1.f);
	EXPECT_FALSE(FogComponent->GetIsVolumetric());

	// Set new values
	FogComponent->SetFogMode(FogMode::Exponential);
	FogComponent->SetPosition(csp::common::Vector3::One());
	FogComponent->SetRotation(csp::common::Vector4(0, 0, 0, 1));
	FogComponent->SetScale(csp::common::Vector3(2, 2, 2));
	FogComponent->SetStartDistance(1.1f);
	FogComponent->SetEndDistance(2.2f);
	FogComponent->SetColor(csp::common::Vector3::One());
	FogComponent->SetDensity(3.3f);
	FogComponent->SetHeightFalloff(4.4f);
	FogComponent->SetMaxOpacity(5.5f);
	FogComponent->SetIsVolumetric(true);

	// Ensure values are set correctly
	EXPECT_EQ(FogComponent->GetFogMode(), FogMode::Exponential);
	EXPECT_EQ(FogComponent->GetPosition(), csp::common::Vector3::One());
	EXPECT_EQ(FogComponent->GetRotation(), csp::common::Vector4(0, 0, 0, 1));
	EXPECT_EQ(FogComponent->GetScale(), csp::common::Vector3(2, 2, 2));
	EXPECT_FLOAT_EQ(FogComponent->GetStartDistance(), 1.1f);
	EXPECT_FLOAT_EQ(FogComponent->GetEndDistance(), 2.2f);
	EXPECT_EQ(FogComponent->GetColor(), csp::common::Vector3::One());
	EXPECT_FLOAT_EQ(FogComponent->GetDensity(), 3.3f);
	EXPECT_FLOAT_EQ(FogComponent->GetHeightFalloff(), 4.4f);
	EXPECT_FLOAT_EQ(FogComponent->GetMaxOpacity(), 5.5f);
	EXPECT_TRUE(FogComponent->GetIsVolumetric());

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_FOG_SCRIPT_INTERFACE_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, FogScriptInterfaceTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	// Log in
	csp::common::String UserId;
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	// Create object to represent the fog
	csp::common::String ObjectName = "Object 1";
	SpaceTransform ObjectTransform = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};
	auto [CreatedObject]		   = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

	// Create fog component
	auto* FogComponent = (FogSpaceComponent*) CreatedObject->AddComponent(ComponentType::Fog);

	CreatedObject->QueueUpdate();
	EntitySystem->ProcessPendingEntityOperations();

	// Setup script
	const std::string FogScriptText = R"xx(
		var fog = ThisEntity.getFogComponents()[0];
		fog.fogMode = 1;
		fog.position = [1, 1, 1];
		fog.rotation = [1, 1, 1, 2];
		fog.scale = [2, 2, 2];
		fog.startDistance = 1.1;
		fog.endDistance = 2.2;
		fog.color = [1, 1, 1];
		fog.density = 3.3;
		fog.heightFalloff = 4.4;
		fog.maxOpacity = 5.5;
		fog.isVolumetric = true;
    )xx";

	CreatedObject->GetScript()->SetScriptSource(FogScriptText.c_str());
	CreatedObject->GetScript()->Invoke();

	EntitySystem->ProcessPendingEntityOperations();

	EXPECT_EQ(FogComponent->GetFogMode(), FogMode::Exponential);
	EXPECT_EQ(FogComponent->GetPosition(), csp::common::Vector3::One());
	EXPECT_EQ(FogComponent->GetRotation(), csp::common::Vector4(1, 1, 1, 2));
	EXPECT_EQ(FogComponent->GetScale(), csp::common::Vector3(2, 2, 2));
	EXPECT_FLOAT_EQ(FogComponent->GetStartDistance(), 1.1f);
	EXPECT_FLOAT_EQ(FogComponent->GetEndDistance(), 2.2f);
	EXPECT_EQ(FogComponent->GetColor(), csp::common::Vector3::One());
	EXPECT_FLOAT_EQ(FogComponent->GetDensity(), 3.3f);
	EXPECT_FLOAT_EQ(FogComponent->GetHeightFalloff(), 4.4f);
	EXPECT_FLOAT_EQ(FogComponent->GetMaxOpacity(), 5.5f);
	EXPECT_TRUE(FogComponent->GetIsVolumetric());

	AWAIT(SpaceSystem, ExitSpaceAndDisconnect, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif


// Disabled by default as it can be slow
#if RUN_MULTIPLAYER_MANYENTITIES_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, ManyEntitiesTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();
	auto* AssetSystem	 = SystemsManager.GetAssetSystem();

	const char* TestSpaceName			= "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription	= "OLY-UNITTEST-SPACEDESC-REWIND";
	const char* TestAssetCollectionName = "OLY-UNITTEST-ASSETCOLLECTION-REWIND";
	const char* TestAssetName			= "OLY-UNITTEST-ASSET-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	EntitySystem->SetEntityCreatedCallback(
		[](auto)
		{
		});

	// Create a bunch of entities
	constexpr size_t NUM_ENTITIES_TO_CREATE = 105;
	constexpr char ENTITY_NAME_PREFIX[]		= "Object_";

	SpaceTransform Transform = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};

	for (size_t i = 0; i < NUM_ENTITIES_TO_CREATE; ++i)
	{
		csp::common::String Name = ENTITY_NAME_PREFIX;
		Name.Append(std::to_string(i).c_str());

		auto [Object] = AWAIT(EntitySystem, CreateObject, Name, Transform);

		EXPECT_NE(Object, nullptr);
	}

	// Clear all entities locally
	auto InternalEntitySystem = static_cast<InternalSpaceEntitySystem*>(EntitySystem);
	InternalEntitySystem->ClearEntities();

	// Retrieve all entities and verify count
	auto GotAllEntities = false;

	EntitySystem->SetInitialEntitiesRetrievedCallback(
		[&](bool)
		{
			GotAllEntities = true;
		});

	EntitySystem->RetrieveAllEntities();

	while (!GotAllEntities)
	{
		std::this_thread::sleep_for(100ms);
	}

	EXPECT_EQ(EntitySystem->GetNumEntities(), NUM_ENTITIES_TO_CREATE);

	// Disconnect from the SignalR server
	AWAIT(SpaceSystem, ExitSpace, Connection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_MULTIPLAYER_BANNED_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, BannedTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();
	auto* AssetSystem	 = SystemsManager.GetAssetSystem();
	auto* SettingsSystem = SystemsManager.GetSettingsSystem();

	const char* TestSpaceName			= "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription	= "OLY-UNITTEST-SPACEDESC-REWIND";
	const char* TestAssetCollectionName = "OLY-UNITTEST-ASSETCOLLECTION-REWIND";
	const char* TestAssetName			= "OLY-UNITTEST-ASSET-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Set username
	{
		auto [Result] = AWAIT_PRE(UserSystem, UpdateUserDisplayName, RequestPredicate, UserId, "Fdn Func Tests");

		EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);
	}

	// Prompt user for space ID
	std::cout << "Space ID: ";
	std::string SpaceId;
	std::cin >> SpaceId;

	auto* Connection   = new csp::multiplayer::MultiplayerConnection(SpaceId.c_str());
	auto* EntitySystem = Connection->GetSpaceEntitySystem();

	EntitySystem->SetEntityCreatedCallback(
		[](auto)
		{
		});

	bool Disconnected = false;

	Connection->SetDisconnectionCallback(
		[&Disconnected](auto Reason)
		{
			FOUNDATION_LOG_MSG(csp::systems::LogLevel::Log, "Disconnected from space. Reason:");
			FOUNDATION_LOG_MSG(csp::systems::LogLevel::Log, Reason);

			Disconnected = true;
		});

	auto [Ok] = AWAIT(SpaceSystem, EnterSpace, SpaceId.c_str();

	EXPECT_TRUE(Ok);

	// Connect to the SignalR server
	std::tie(Ok) = AWAIT(Connection, Connect);

	EXPECT_TRUE(Ok);

	std::tie(Ok) = AWAIT(Connection, InitialiseConnection);

	EXPECT_TRUE(Ok);

	// Create avatar
	const SpaceTransform& UserTransform = {csp::common::Vector3::Zero(), csp::common::Vector4::Identity(), csp::common::Vector3::One()};
	auto [Avatar] = AWAIT(EntitySystem, CreateAvatar, "Other Michael", UserTransform, AvatarState::Idle, UserId, AvatarPlayMode::Default);

	EXPECT_NE(Avatar, nullptr);

	// Set extra avatar properties
	auto* AvatarComponent = (csp::multiplayer::AvatarSpaceComponent*) Avatar->GetComponent(0);
	AvatarComponent->SetAvatarMeshIndex(4);
	AvatarComponent->SetUserId(UserId);

	Avatar->QueueUpdate();

	// Tick and send movement updates until disconnected
	auto SleepTime = 0;

	while (!Disconnected)
	{
		std::this_thread::sleep_for(10ms);

		SleepTime += 10;

		if (SleepTime == 50) // Send a movement update roughly every 50ms
		{
			Avatar->SetPosition({(float) (rand() % 5) - 2.5f, 0, (float) (rand() % 5) - 2.5f});
			Avatar->QueueUpdate();
			SleepTime = 0;
		}

		csp::CSPFoundation::Tick();
	}

	// Disconnect from the SignalR server
	std::tie(Ok) = AWAIT(Connection, Disconnect);

	EXPECT_TRUE(Ok);

	SpaceSystem->ExitSpace();

	// Delete MultiplayerConnection
	delete Connection;

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_MULTIPLAYER_TESTS || RUN_MULTIPLAYER_INVALID_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, InvalidComponentFieldsTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();
	auto* AssetSystem	 = SystemsManager.GetAssetSystem();

	const char* TestSpaceName			= "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription	= "OLY-UNITTEST-SPACEDESC-REWIND";
	const char* TestAssetCollectionName = "OLY-UNITTEST-ASSETCOLLECTION-REWIND";
	const char* TestAssetName			= "OLY-UNITTEST-ASSET-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	bool AssetDetailBlobChangedCallbackCalled = false;
	csp::common::String CallbackAssetId;

	const csp::common::String ObjectName = "Object 1";
	SpaceTransform ObjectTransform		 = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};

	auto [Object] = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

	const csp::common::String ModelAssetId = "NotARealId";

	auto* LightSpaceComponentInstance = (LightSpaceComponent*) Object->AddComponent(ComponentType::Invalid);

	// Process component creation
	Object->QueueUpdate();
	EntitySystem->ProcessPendingEntityOperations();
	// Disconnect from the SignalR server

	auto [Ok] = AWAIT(Connection, Disconnect);

	EXPECT_TRUE(Ok);

	SpaceSystem->ExitSpace();

	// Delete MultiplayerConnection
	delete Connection;

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_SCRIPTSYSTEM_TESTS || RUN_MULTIPLAYER_MULTIPLE_SCRIPT_COMPONENT_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, MultipleScriptComponentTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	// Enter space
	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	// Create space object
	csp::common::String ObjectName = "Object 1";
	SpaceTransform ObjectTransform = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};
	auto [SpaceEntity]			   = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

	// Attempt to add 2 script components
	auto Comp1 = SpaceEntity->AddComponent(csp::multiplayer::ComponentType::ScriptData);
	auto Comp2 = SpaceEntity->AddComponent(csp::multiplayer::ComponentType::ScriptData);

	SpaceEntity->QueueUpdate();
	EntitySystem->ProcessPendingEntityOperations();

	// Only 1 script component should be on the object
	EXPECT_TRUE(SpaceEntity->GetComponents()->Size(), 1);

	// Disconnect from the SignalR server
	auto [Ok] = AWAIT(Connection, Disconnect);

	EXPECT_TRUE(Ok);

	SpaceSystem->ExitSpace();

	// Delete MultiplayerConnection
	delete Connection;

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_SCRIPTSYSTEM_TESTS || RUN_MULTIPLAYER_FIND_COMPONENT_BY_ID_TEST
CSP_PUBLIC_TEST(CSPEngine, MultiplayerTests, FindComponentByIdTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();

	const char* TestSpaceName		 = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription = "OLY-UNITTEST-SPACEDESC-REWIND";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	// Enter space
	auto [EnterResult] = AWAIT_PRE(SpaceSystem, EnterSpace, RequestPredicate, Space.Id, true);
	EXPECT_EQ(EnterResult.GetResultCode(), csp::services::EResultCode::Success);
	Connection	 = EnterResult.GetConnection();
	EntitySystem = Connection->GetSpaceEntitySystem();

	EntitySystem->SetEntityCreatedCallback(
		[](SpaceEntity* Entity)
		{
		});

	// Create space object
	csp::common::String ObjectName = "Object 1";
	SpaceTransform ObjectTransform = {csp::common::Vector3::Zero(), csp::common::Vector4::Zero(), csp::common::Vector3::One()};
	auto [SpaceEntity]			   = AWAIT(EntitySystem, CreateObject, ObjectName, ObjectTransform);

	// Create second space object
	csp::common::String ObjectName2 = "Object 2";
	auto [SpaceEntity2]				= AWAIT(EntitySystem, CreateObject, ObjectName2, ObjectTransform);

	auto Component1 = SpaceEntity->AddComponent(ComponentType::AnimatedModel);
	auto Component2 = SpaceEntity2->AddComponent(ComponentType::AnimatedModel);

	SpaceEntity->QueueUpdate();
	SpaceEntity2->QueueUpdate();
	EntitySystem->ProcessPendingEntityOperations();

	auto FoundComponent = EntitySystem->FindComponentById(Component1->GetId());

	EXPECT_TRUE(FoundComponent != nullptr);
	EXPECT_EQ(Component1->GetId(), FoundComponent->GetId());

	FoundComponent = EntitySystem->FindComponentById(Component2->GetId());

	EXPECT_TRUE(FoundComponent != nullptr);
	EXPECT_EQ(Component2->GetId(), FoundComponent->GetId());

	// Disconnect from the SignalR server
	auto [Ok] = AWAIT(Connection, Disconnect);

	EXPECT_TRUE(Ok);

	SpaceSystem->ExitSpace();

	// Delete MultiplayerConnection
	delete Connection;

	// Log out
	LogOut(UserSystem);
}
#endif

} // namespace