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
#include "CSP/Common/Array.h"
#include "CSP/Systems/Assets/AssetSystem.h"
#include "CSP/Systems/Spaces/SpaceSystem.h"
#include "CSP/Systems/SystemsManager.h"
#include "SpaceSystemTestHelpers.h"
#include "TestHelpers.h"
#include "UserSystemTestHelpers.h"

/**/
#include <Awaitable.h>
/**/

#include "gtest/gtest.h"
#include <filesystem>

namespace
{

bool RequestPredicate(const csp::services::ResultBase& Result)
{
	return Result.GetResultCode() != csp::services::EResultCode::InProgress;
}

bool RequestPredicateWithProgress(const csp::services::ResultBase& Result)
{
	if (Result.GetResultCode() == csp::services::EResultCode::InProgress)
	{
		PrintProgress(Result.GetRequestProgress());

		return false;
	}

	return true;
}

} // namespace

void CreateAssetCollection(csp::systems::AssetSystem* AssetSystem,
						   const csp::common::Optional<csp::common::String>& SpaceId,
						   const csp::common::Optional<csp::common::String>& ParentId,
						   const csp::common::String& Name,
						   const csp::common::Optional<csp::systems::EAssetCollectionType>& AssetCollectionType,
						   const csp::common::Optional<csp::common::Array<csp::common::String>>& Tags,
						   csp::systems::AssetCollection& OutAssetCollection)
{
	csp::systems::EAssetCollectionType type = csp::systems::EAssetCollectionType::DEFAULT;

	if (AssetCollectionType.HasValue())
	{
		type = *AssetCollectionType;
	}

	auto [Result] = Awaitable(&csp::systems::AssetSystem::CreateAssetCollection, AssetSystem, SpaceId, ParentId, Name, nullptr, type, Tags)
						.Await(RequestPredicate);

	EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);

	OutAssetCollection = Result.GetAssetCollection();
}

void DeleteAssetCollection(csp::systems::AssetSystem* AssetSystem, csp::systems::AssetCollection& AssetCollection)
{
	auto [Result] = Awaitable(&csp::systems::AssetSystem::DeleteAssetCollection, AssetSystem, AssetCollection).Await(RequestPredicate);

	EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);
}

void GetAssetCollections(csp::systems::AssetSystem* AssetSystem,
						 csp::systems::Space& Space,
						 csp::common::Array<csp::systems::AssetCollection>& OutAssetCollections)
{
	auto [Result] = Awaitable(&csp::systems::AssetSystem::GetAssetCollectionsByCriteria,
							  AssetSystem,
							  Space.Id,
							  nullptr,
							  csp::systems::EAssetCollectionType::DEFAULT,
							  nullptr,
							  nullptr,
							  nullptr,
							  nullptr)
						.Await(RequestPredicate);

	EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);

	const auto& AssetCollections = Result.GetAssetCollections();
	OutAssetCollections			 = csp::common::Array<csp::systems::AssetCollection>(AssetCollections.Size());

	for (size_t i = 0U; i < AssetCollections.Size(); ++i)
	{
		OutAssetCollections[i] = AssetCollections[i];
	}
}

void GetAssetCollectionByName(csp::systems::AssetSystem* AssetSystem,
							  const csp::common::String& AssetCollectionName,
							  csp::systems::AssetCollection& OutAssetCollection)
{
	auto [Result] = Awaitable(&csp::systems::AssetSystem::GetAssetCollectionByName, AssetSystem, AssetCollectionName).Await(RequestPredicate);

	EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);

	OutAssetCollection = Result.GetAssetCollection();
}


void GetAssetCollectionsByIds(csp::systems::AssetSystem* AssetSystem,
							  const csp::common::Array<csp::common::String>& Ids,
							  csp::common::Array<csp::systems::AssetCollection>& OutAssetCollections)
{
	EXPECT_FALSE(Ids.IsEmpty());

	auto [Result] = Awaitable(&csp::systems::AssetSystem::GetAssetCollectionsByIds, AssetSystem, Ids).Await(RequestPredicate);

	EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);

	const auto& AssetCollections = Result.GetAssetCollections();
	OutAssetCollections			 = csp::common::Array<csp::systems::AssetCollection>(AssetCollections.Size());

	for (size_t i = 0U; i < AssetCollections.Size(); ++i)
	{
		OutAssetCollections[i] = AssetCollections[i];
	}
}

void CreateAsset(csp::systems::AssetSystem* AssetSystem,
				 csp::systems::AssetCollection& AssetCollection,
				 const csp::common::String& Name,
				 const csp::common::Optional<csp::common::String>& ThirdPartyPackagedAssetIdentifier,
				 const csp::common::Optional<csp::systems::EThirdPartyPlatform>& ThirdPartyPlatform,
				 csp::systems::Asset& OutAsset)
{
	auto [Result] = Awaitable(&csp::systems::AssetSystem::CreateAsset,
							  AssetSystem,
							  AssetCollection,
							  Name,
							  ThirdPartyPackagedAssetIdentifier,
							  ThirdPartyPlatform,
							  csp::systems::EAssetType::MODEL)
						.Await(RequestPredicate);

	EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);

	OutAsset = Result.GetAsset();
}

void UploadAssetData(csp::systems::AssetSystem* AssetSystem,
					 const csp::systems::AssetCollection& AssetCollection,
					 const csp::systems::Asset& Asset,
					 const csp::systems::FileAssetDataSource& Source,
					 csp::common::String& OutUri)
{
	auto [Result] = AWAIT_PRE(AssetSystem, UploadAssetData, RequestPredicateWithProgress, AssetCollection, Asset, Source);

	EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);

	OutUri = Result.GetUri();
}

void UploadAssetData(csp::systems::AssetSystem* AssetSystem,
					 const csp::systems::AssetCollection& AssetCollection,
					 const csp::systems::Asset& Asset,
					 const csp::systems::BufferAssetDataSource& Source,
					 csp::common::String& OutUri)
{
	auto [Result] = AWAIT_PRE(AssetSystem, UploadAssetData, RequestPredicateWithProgress, AssetCollection, Asset, Source);

	EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);

	OutUri = Result.GetUri();
}

void GetAssetById(csp::systems::AssetSystem* AssetSystem,
				  const csp::common::String& AssetCollectionId,
				  const csp::common::String& AssetId,
				  csp::systems::Asset& OutAsset)
{
	auto [Result] = AWAIT_PRE(AssetSystem, GetAssetById, RequestPredicate, AssetCollectionId, AssetId);

	EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);

	OutAsset = Result.GetAsset();
}

void DeleteAsset(csp::systems::AssetSystem* AssetSystem, csp::systems::AssetCollection& AssetCollection, csp::systems::Asset& Asset)
{
	auto [Result] = Awaitable(&csp::systems::AssetSystem::DeleteAsset, AssetSystem, AssetCollection, Asset).Await(RequestPredicate);

	EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);
}

void UpdateAsset(csp::systems::AssetSystem* AssetSystem, csp::systems::AssetCollection& AssetCollection, csp::systems::Asset& Asset)
{
	auto [Result] = Awaitable(&csp::systems::AssetSystem::UpdateAsset, AssetSystem, Asset).Await(RequestPredicate);

	EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);
}

void GetAssetsInCollection(csp::systems::AssetSystem* AssetSystem,
						   csp::systems::AssetCollection& AssetCollection,
						   csp::common::Array<csp::systems::Asset>& OutAssets)
{
	auto [Result] = Awaitable(&csp::systems::AssetSystem::GetAssetsInCollection, AssetSystem, AssetCollection).Await(RequestPredicate);

	EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);

	const auto& Assets = Result.GetAssets();
	OutAssets		   = csp::common::Array<csp::systems::Asset>(Assets.Size());

	for (size_t i = 0U; i < Assets.Size(); ++i)
	{
		OutAssets[i] = Assets[i];
	}
}

void GetAssetsByCollectionIds(csp::systems::AssetSystem* AssetSystem,
							  const csp::common::Array<csp::common::String>& Ids,
							  csp::common::Array<csp::systems::Asset>& OutAssets)
{
	EXPECT_FALSE(Ids.IsEmpty());

	auto [Result] = Awaitable(&csp::systems::AssetSystem::GetAssetsByCollectionIds, AssetSystem, Ids).Await(RequestPredicate);

	EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);

	const auto& Assets = Result.GetAssets();
	OutAssets		   = csp::common::Array<csp::systems::Asset>(Assets.Size());

	for (size_t i = 0U; i < Assets.Size(); ++i)
	{
		OutAssets[i] = Assets[i];
	}
}

void UpdateAssetCollectionMetadata(csp::systems::AssetSystem* AssetSystem,
								   csp::systems::AssetCollection& AssetCollection,
								   const csp::common::Map<csp::common::String, csp::common::String>& InMetaData,
								   csp::common::Map<csp::common::String, csp::common::String>& OutMetaData)
{
	auto [Result]
		= Awaitable(&csp::systems::AssetSystem::UpdateAssetCollectionMetadata, AssetSystem, AssetCollection, InMetaData).Await(RequestPredicate);
	auto ResultAssetCollection = Result.GetAssetCollection();

	// Check Result Data has only changed MetData
	EXPECT_EQ(ResultAssetCollection.Id, AssetCollection.Id);

	EXPECT_EQ(ResultAssetCollection.ParentId, AssetCollection.ParentId);

	EXPECT_EQ(ResultAssetCollection.Name, AssetCollection.Name);

	EXPECT_EQ(ResultAssetCollection.Id, AssetCollection.Id);

	EXPECT_NE(ResultAssetCollection.UpdatedAt, AssetCollection.UpdatedAt);

	auto Tags = ResultAssetCollection.Tags;

	for (size_t i = 0U; i < Tags.Size(); ++i)
	{
		EXPECT_EQ(Tags[i], AssetCollection.Tags[i]);
	}

	EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);


	OutMetaData = ResultAssetCollection.GetMetadataImmutable();
}


#if RUN_ALL_UNIT_TESTS || RUN_ASSETSYSTEM_TESTS || RUN_ASSETSYSTEM_CREATEASSETCOLLECTION_TEST
CSP_PUBLIC_TEST(CSPEngine, AssetSystemTests, CreateAssetCollectionTest)
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

	// Create asset collection
	csp::systems::AssetCollection AssetCollection;
	CreateAssetCollection(AssetSystem, Space.Id, nullptr, UniqueAssetCollectionName, nullptr, nullptr, AssetCollection);

	// Get asset collections
	csp::common::Array<csp::systems::AssetCollection> AssetCollections;
	GetAssetCollections(AssetSystem, Space, AssetCollections);

	EXPECT_EQ(AssetCollections.Size(), 1);
	EXPECT_EQ(AssetCollections[0].Name, UniqueAssetCollectionName);

	// Delete asset collection
	DeleteAssetCollection(AssetSystem, AssetCollection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_ASSETSYSTEM_TESTS || RUN_ASSETSYSTEM_CREATEASSETCOLLECTION_NOSPACE_TEST
CSP_PUBLIC_TEST(CSPEngine, AssetSystemTests, CreateAssetCollectionNoSpaceTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();
	auto* AssetSystem	 = SystemsManager.GetAssetSystem();

	const char* TestAssetCollectionName = "OLY-UNITTEST-ASSETCOLLECTION-REWIND";

	char UniqueAssetCollectionName[256];
	SPRINTF(UniqueAssetCollectionName, "%s-%s", TestAssetCollectionName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create asset collection
	csp::systems::AssetCollection NewAssetCollection;
	CreateAssetCollection(AssetSystem, nullptr, nullptr, UniqueAssetCollectionName, nullptr, nullptr, NewAssetCollection);

	// Get asset collections
	csp::systems::AssetCollection AssetCollection;
	GetAssetCollectionByName(AssetSystem, UniqueAssetCollectionName, AssetCollection);

	EXPECT_EQ(AssetCollection.Name, UniqueAssetCollectionName);
	EXPECT_TRUE(AssetCollection.SpaceIds.IsEmpty());

	// Delete asset collection
	DeleteAssetCollection(AssetSystem, NewAssetCollection);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_ASSETSYSTEM_TESTS || RUN_ASSETSYSTEM_GETASSETCOLLECTIONSBYIDS_TEST
CSP_PUBLIC_TEST(CSPEngine, AssetSystemTests, GetAssetCollectionsByIdsTest)
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

	char UniqueAssetCollectionName1[256];
	SPRINTF(UniqueAssetCollectionName1, "%s-%s", TestAssetCollectionName, GetUniqueHexString().c_str());

	char UniqueAssetCollectionName2[256];
	SPRINTF(UniqueAssetCollectionName2, "%s-%s", TestAssetCollectionName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	// Create asset collections
	csp::systems::AssetCollection AssetCollection1, AssetCollection2;
	CreateAssetCollection(AssetSystem, Space.Id, nullptr, UniqueAssetCollectionName1, nullptr, nullptr, AssetCollection1);
	CreateAssetCollection(AssetSystem, Space.Id, nullptr, UniqueAssetCollectionName2, nullptr, nullptr, AssetCollection2);

	// Get asset collections
	csp::common::Array<csp::systems::AssetCollection> AssetCollections;
	GetAssetCollectionsByIds(AssetSystem, {AssetCollection1.Id, AssetCollection2.Id}, AssetCollections);

	EXPECT_EQ(AssetCollections.Size(), 2);

	bool Found1 = false, Found2 = false;

	for (int i = 0; i < AssetCollections.Size(); ++i)
	{
		auto& AssetCollection = AssetCollections[i];

		if (AssetCollection.Id == AssetCollection1.Id)
		{
			Found1 = true;
		}
		else if (AssetCollection.Id == AssetCollection2.Id)
		{
			Found2 = true;
		}
	}

	EXPECT_TRUE(Found1 && Found2);

	// Delete asset collections
	DeleteAssetCollection(AssetSystem, AssetCollection1);
	DeleteAssetCollection(AssetSystem, AssetCollection2);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif


#if RUN_ALL_UNIT_TESTS || RUN_ASSETSYSTEM_TESTS || RUN_ASSETSYSTEM_CREATEASSET_TEST
CSP_PUBLIC_TEST(CSPEngine, AssetSystemTests, CreateAssetTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();
	auto* AssetSystem	 = SystemsManager.GetAssetSystem();

	const char* TestSpaceName			  = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription	  = "OLY-UNITTEST-SPACEDESC-REWIND";
	const char* TestAssetCollectionName	  = "OLY-UNITTEST-ASSETCOLLECTION-REWIND";
	const char* TestAssetName			  = "OLY-UNITTEST-ASSET-REWIND";
	const char* TestThirdPartyReferenceId = "OLY-UNITTEST-ASSET-THIRDPARTY";

	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	char UniqueAssetCollectionName[256];
	SPRINTF(UniqueAssetCollectionName, "%s-%s", TestAssetCollectionName, GetUniqueHexString().c_str());

	char UniqueAssetName[256];
	SPRINTF(UniqueAssetName, "%s-%s", TestAssetName, GetUniqueHexString().c_str());

	csp::common::String ThirdPartyPackagedAssetIdentifier;
	ThirdPartyPackagedAssetIdentifier = "OKO interoperable assets Test";

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	std::cout << UserId << "\n";

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	// Create asset collection
	csp::systems::AssetCollection AssetCollection;
	CreateAssetCollection(AssetSystem, Space.Id, nullptr, UniqueAssetCollectionName, nullptr, nullptr, AssetCollection);

	// Create asset
	csp::systems::Asset Asset;
	CreateAsset(AssetSystem, AssetCollection, UniqueAssetName, ThirdPartyPackagedAssetIdentifier, nullptr, Asset);

	// Get assets
	csp::common::Array<csp::systems::Asset> Assets;
	GetAssetsInCollection(AssetSystem, AssetCollection, Assets);

	EXPECT_EQ(Assets.Size(), 1);
	EXPECT_EQ(Assets[0].Name, UniqueAssetName);
	EXPECT_EQ(Assets[0].GetThirdPartyPackagedAssetIdentifier(), ThirdPartyPackagedAssetIdentifier);

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

#if RUN_ALL_UNIT_TESTS || RUN_ASSETSYSTEM_TESTS || RUN_ASSETSYSTEM_CREATEASSET_NOSPACE_TEST
CSP_PUBLIC_TEST(CSPEngine, AssetSystemTests, CreateAssetNoSpaceTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();
	auto* AssetSystem	 = SystemsManager.GetAssetSystem();

	const char* TestAssetCollectionName	  = "OLY-UNITTEST-ASSETCOLLECTION-REWIND";
	const char* TestAssetName			  = "OLY-UNITTEST-ASSET-REWIND";
	const char* TestThirdPartyReferenceId = "OLY-UNITTEST-ASSET-THIRDPARTY";

	char UniqueAssetCollectionName[256];
	SPRINTF(UniqueAssetCollectionName, "%s-%s", TestAssetCollectionName, GetUniqueHexString().c_str());

	char UniqueAssetName[256];
	SPRINTF(UniqueAssetName, "%s-%s", TestAssetName, GetUniqueHexString().c_str());

	csp::common::String ThirdPartyPackagedAssetIdentifier;
	ThirdPartyPackagedAssetIdentifier = "OKO interoperable assets Test";

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	std::cout << UserId << "\n";

	// Create asset collection
	csp::systems::AssetCollection AssetCollection;
	CreateAssetCollection(AssetSystem, nullptr, nullptr, UniqueAssetCollectionName, nullptr, nullptr, AssetCollection);

	// Create asset
	csp::systems::Asset Asset;
	CreateAsset(AssetSystem, AssetCollection, UniqueAssetName, ThirdPartyPackagedAssetIdentifier, nullptr, Asset);

	// Get assets
	csp::common::Array<csp::systems::Asset> Assets;
	GetAssetsInCollection(AssetSystem, AssetCollection, Assets);

	EXPECT_EQ(Assets.Size(), 1);
	EXPECT_EQ(Assets[0].Name, UniqueAssetName);
	EXPECT_EQ(Assets[0].GetThirdPartyPackagedAssetIdentifier(), ThirdPartyPackagedAssetIdentifier);

	// Delete asset
	DeleteAsset(AssetSystem, AssetCollection, Asset);

	// Delete asset collection
	DeleteAssetCollection(AssetSystem, AssetCollection);

	// Log out
	LogOut(UserSystem);
}
#endif


#if RUN_ALL_UNIT_TESTS || RUN_ASSETSYSTEM_TESTS || RUN_ASSETSYSTEM_UPDATEXTERNALURIEASSET_TEST
CSP_PUBLIC_TEST(CSPEngine, AssetSystemTests, UpdateExternalUriAssetTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* SpaceSystem	 = SystemsManager.GetSpaceSystem();
	auto* AssetSystem	 = SystemsManager.GetAssetSystem();

	const char* TestSpaceName			  = "OLY-UNITTEST-SPACE-REWIND";
	const char* TestSpaceDescription	  = "OLY-UNITTEST-SPACEDESC-REWIND";
	const char* TestAssetCollectionName	  = "OLY-UNITTEST-ASSETCOLLECTION-REWIND";
	const char* TestAssetName			  = "OLY-UNITTEST-ASSET-REWIND";
	const char* TestThirdPartyReferenceId = "OLY-UNITTEST-ASSET-THIRDPARTY";
	const char* TestExternalUri			  = "https://github.com/KhronosGroup/glTF-Sample-Models/raw/master/2.0/Duck/glTF-Binary/Duck.glb";
	const char* TestExternalMimeType	  = "model/gltf-binary";
	char UniqueSpaceName[256];
	SPRINTF(UniqueSpaceName, "%s-%s", TestSpaceName, GetUniqueHexString().c_str());

	char UniqueAssetCollectionName[256];
	SPRINTF(UniqueAssetCollectionName, "%s-%s", TestAssetCollectionName, GetUniqueHexString().c_str());

	char UniqueAssetName[256];
	SPRINTF(UniqueAssetName, "%s-%s", TestAssetName, GetUniqueHexString().c_str());

	csp::common::String ThirdPartyPackagedAssetIdentifier;
	ThirdPartyPackagedAssetIdentifier = "OKO interoperable assets Test";

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	// Create asset collection
	csp::systems::AssetCollection AssetCollection;
	CreateAssetCollection(AssetSystem, Space.Id, nullptr, UniqueAssetCollectionName, nullptr, nullptr, AssetCollection);

	// Create asset
	csp::systems::Asset Asset;
	CreateAsset(AssetSystem, AssetCollection, UniqueAssetName, ThirdPartyPackagedAssetIdentifier, nullptr, Asset);

	// Get assets
	csp::common::Array<csp::systems::Asset> Assets;
	GetAssetsInCollection(AssetSystem, AssetCollection, Assets);


	EXPECT_EQ(Assets.Size(), 1);
	EXPECT_EQ(Assets[0].Name, UniqueAssetName);
	EXPECT_EQ(Assets[0].GetThirdPartyPackagedAssetIdentifier(), ThirdPartyPackagedAssetIdentifier);
	EXPECT_EQ(Assets[0].Uri, "");

	Assets[0].ExternalUri	   = TestExternalUri;
	Assets[0].ExternalMimeType = TestExternalMimeType;

	auto [Result] = Awaitable(&csp::systems::AssetSystem::UpdateAsset, AssetSystem, Assets[0]).Await(RequestPredicate);

	EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);

	// Get assets
	GetAssetsInCollection(AssetSystem, AssetCollection, Assets);


	EXPECT_EQ(Result.GetAsset().Uri, TestExternalUri);
	EXPECT_EQ(Result.GetAsset().MimeType, TestExternalMimeType);

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


#if RUN_ALL_UNIT_TESTS || RUN_ASSETSYSTEM_TESTS || RUN_ASSETSYSTEM_GETASSETSBYCOLLECTIONIDS_TEST
CSP_PUBLIC_TEST(CSPEngine, AssetSystemTests, GetAssetsByCollectionIdsTest)
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

	char UniqueAssetCollectionName1[256], UniqueAssetCollectionName2[256];
	SPRINTF(UniqueAssetCollectionName1, "%s-%s", TestAssetCollectionName, GetUniqueHexString().c_str());
	SPRINTF(UniqueAssetCollectionName2, "%s-%s", TestAssetCollectionName, GetUniqueHexString().c_str());

	char UniqueAssetName1[256], UniqueAssetName2[256], UniqueAssetName3[256];
	SPRINTF(UniqueAssetName1, "%s-%s", TestAssetName, GetUniqueHexString().c_str());
	SPRINTF(UniqueAssetName2, "%s-%s", TestAssetName, GetUniqueHexString().c_str());
	SPRINTF(UniqueAssetName3, "%s-%s", TestAssetName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	// Create asset collections
	csp::systems::AssetCollection AssetCollection1, AssetCollection2;
	CreateAssetCollection(AssetSystem, Space.Id, nullptr, UniqueAssetCollectionName1, nullptr, nullptr, AssetCollection1);
	CreateAssetCollection(AssetSystem, Space.Id, nullptr, UniqueAssetCollectionName2, nullptr, nullptr, AssetCollection2);

	// Create assets
	csp::systems::Asset Asset1, Asset2, Asset3;
	CreateAsset(AssetSystem, AssetCollection1, UniqueAssetName1, nullptr, nullptr, Asset1);
	CreateAsset(AssetSystem, AssetCollection1, UniqueAssetName2, nullptr, nullptr, Asset2);
	CreateAsset(AssetSystem, AssetCollection2, UniqueAssetName3, nullptr, nullptr, Asset3);

	// Get assets
	csp::common::Array<csp::systems::Asset> Assets;
	GetAssetsByCollectionIds(AssetSystem, {AssetCollection1.Id, AssetCollection2.Id}, Assets);

	EXPECT_EQ(Assets.Size(), 3);

	bool Found1 = false, Found2 = false, Found3 = false;

	for (int i = 0; i < Assets.Size(); ++i)
	{
		auto& Asset = Assets[i];

		if (Asset.Id == Asset1.Id)
		{
			Found1 = true;
		}
		else if (Asset.Id == Asset2.Id)
		{
			Found2 = true;
		}
		else if (Asset.Id == Asset3.Id)
		{
			Found3 = true;
		}
	}

	EXPECT_TRUE(Found1 && Found2 && Found3);

	// Delete assets
	DeleteAsset(AssetSystem, AssetCollection2, Asset3);
	DeleteAsset(AssetSystem, AssetCollection1, Asset2);
	DeleteAsset(AssetSystem, AssetCollection1, Asset1);

	// Delete asset collections
	DeleteAssetCollection(AssetSystem, AssetCollection2);
	DeleteAssetCollection(AssetSystem, AssetCollection1);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif

#if RUN_ALL_UNIT_TESTS || RUN_ASSETSYSTEM_TESTS || RUN_ASSETSYSTEM_GETASSETCOLLECTIONS_BY_DIFFERENT_CRITERIA_TEST
CSP_PUBLIC_TEST(CSPEngine, AssetSystemTests, GetAssetCollectionsByDifferentCriteriaTest)
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

	char UniqueAssetCollectionName1[256];
	SPRINTF(UniqueAssetCollectionName1, "%s-%s", TestAssetCollectionName, GetUniqueHexString().c_str());

	char UniqueAssetCollectionName2[256];
	SPRINTF(UniqueAssetCollectionName2, "%s-%s", TestAssetCollectionName, GetUniqueHexString().c_str());

	char UniqueAssetCollectionName3[256];
	SPRINTF(UniqueAssetCollectionName3, "%s-%s", TestAssetCollectionName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	LogIn(UserSystem, UserId);

	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);
	const auto Tag = csp::common::Array<csp::common::String>({Space.Id});

	csp::systems::AssetCollection AssetCollection1, AssetCollection2, AssetCollection3;
	CreateAssetCollection(AssetSystem,
						  Space.Id,
						  nullptr,
						  UniqueAssetCollectionName1,
						  csp::systems::EAssetCollectionType::SPACE_THUMBNAIL,
						  nullptr,
						  AssetCollection1);
	CreateAssetCollection(AssetSystem,
						  Space.Id,
						  nullptr,
						  UniqueAssetCollectionName2,
						  csp::systems::EAssetCollectionType::SPACE_THUMBNAIL,
						  Tag,
						  AssetCollection2);
	CreateAssetCollection(AssetSystem, Space.Id, AssetCollection1.Id, UniqueAssetCollectionName3, nullptr, nullptr, AssetCollection3);

	{
		// search by space
		auto [Result]
			= AWAIT_PRE(AssetSystem, GetAssetCollectionsByCriteria, RequestPredicate, Space.Id, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
		EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);
		EXPECT_EQ(Result.GetAssetCollections().Size(), 4);
	}
	{
		// search by parentId
		auto [Result] = AWAIT_PRE(AssetSystem,
								  GetAssetCollectionsByCriteria,
								  RequestPredicate,
								  nullptr,
								  AssetCollection1.Id,
								  nullptr,
								  nullptr,
								  nullptr,
								  nullptr,
								  nullptr);
		EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);
		EXPECT_EQ(Result.GetAssetCollections().Size(), 1);
		EXPECT_EQ(Result.GetAssetCollections()[0].Id, AssetCollection3.Id);
		EXPECT_EQ(Result.GetAssetCollections()[0].Name, AssetCollection3.Name);
	}
	{
		// search by Tag
		auto [Result]
			= AWAIT_PRE(AssetSystem, GetAssetCollectionsByCriteria, RequestPredicate, nullptr, nullptr, nullptr, Tag, nullptr, nullptr, nullptr);
		EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);
		EXPECT_EQ(Result.GetAssetCollections().Size(), 1);
		EXPECT_EQ(Result.GetAssetCollections()[0].Id, AssetCollection2.Id);
		EXPECT_EQ(Result.GetAssetCollections()[0].Name, AssetCollection2.Name);
	}
	{
		// search by names and types
		csp::common::Array<csp::common::String> AssetNames = {UniqueAssetCollectionName1, UniqueAssetCollectionName2};

		// search for Default types with these names
		auto [EmptyResult] = AWAIT_PRE(AssetSystem,
									   GetAssetCollectionsByCriteria,
									   RequestPredicate,
									   nullptr,
									   nullptr,
									   csp::systems::EAssetCollectionType::DEFAULT,
									   nullptr,
									   AssetNames,
									   nullptr,
									   nullptr);
		EXPECT_EQ(EmptyResult.GetResultCode(), csp::services::EResultCode::Success);
		EXPECT_EQ(EmptyResult.GetAssetCollections().Size(), 0);

		// next, search names and space thumbnail type
		auto [Result] = AWAIT_PRE(AssetSystem,
								  GetAssetCollectionsByCriteria,
								  RequestPredicate,
								  nullptr,
								  nullptr,
								  csp::systems::EAssetCollectionType::SPACE_THUMBNAIL,
								  nullptr,
								  AssetNames,
								  nullptr,
								  nullptr);
		EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);
		EXPECT_EQ(Result.GetAssetCollections().Size(), 2);

		bool FoundFirstAssetCollection = false, FoundSecondAssetCollection = false;

		const auto& RetrievedAssetCollections = Result.GetAssetCollections();

		for (int idx = 0; idx < RetrievedAssetCollections.Size(); ++idx)
		{
			auto& CurrentAsset = RetrievedAssetCollections[idx];

			if (CurrentAsset.Id == AssetCollection1.Id)
			{
				FoundFirstAssetCollection = true;
			}
			else if (CurrentAsset.Id == AssetCollection2.Id)
			{
				FoundSecondAssetCollection = true;
			}
		}

		EXPECT_EQ(FoundFirstAssetCollection && FoundSecondAssetCollection, true);
	}
	{
		// Test Pagination,
		auto [Result] = AWAIT_PRE(AssetSystem, GetAssetCollectionsByCriteria, RequestPredicate, Space.Id, nullptr, nullptr, nullptr, nullptr, 1, 1);
		EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);
		EXPECT_EQ(Result.GetAssetCollections().Size(), 1);
	}

	DeleteAssetCollection(AssetSystem, AssetCollection3);
	DeleteAssetCollection(AssetSystem, AssetCollection1);
	DeleteAssetCollection(AssetSystem, AssetCollection2);

	DeleteSpace(SpaceSystem, Space.Id);

	LogOut(UserSystem);
}
#endif


#if RUN_ALL_UNIT_TESTS || RUN_ASSETSYSTEM_TESTS || RUN_ASSETSYSTEM_GETASSETS_BY_DIFFERENT_CRITERIA_TEST
CSP_PUBLIC_TEST(CSPEngine, AssetSystemTests, GetAssetsByDifferentCriteriaTest)
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

	char UniqueFirstAssetName[256];
	SPRINTF(UniqueFirstAssetName, "%s-%s", TestAssetName, GetUniqueHexString().c_str());

	char UniqueSecondAssetName[256];
	SPRINTF(UniqueSecondAssetName, "%s-%s", TestAssetName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	LogIn(UserSystem, UserId);

	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	csp::systems::AssetCollection AssetCollection;
	CreateAssetCollection(AssetSystem, Space.Id, nullptr, UniqueAssetCollectionName, nullptr, nullptr, AssetCollection);

	csp::systems::Asset FirstAsset;
	CreateAsset(AssetSystem, AssetCollection, UniqueFirstAssetName, nullptr, nullptr, FirstAsset);

	csp::systems::Asset SecondAsset;
	CreateAsset(AssetSystem, AssetCollection, UniqueSecondAssetName, nullptr, nullptr, SecondAsset);

	{
		// search by asset id
		csp::common::Array<csp::common::String> AssetIds = {FirstAsset.Id};
		auto [Result] = AWAIT_PRE(AssetSystem, GetAssetsByCriteria, RequestPredicate, {AssetCollection.Id}, AssetIds, nullptr, nullptr);
		EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);
		EXPECT_EQ(Result.GetAssets().Size(), 1);
		EXPECT_EQ(Result.GetAssets()[0].Id, FirstAsset.Id);
		EXPECT_EQ(Result.GetAssets()[0].Name, FirstAsset.Name);
	}
	{
		// search by asset name
		csp::common::Array<csp::common::String> AssetNames = {FirstAsset.Name};
		auto [Result] = AWAIT_PRE(AssetSystem, GetAssetsByCriteria, RequestPredicate, {AssetCollection.Id}, nullptr, AssetNames, nullptr);
		EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);
		EXPECT_EQ(Result.GetAssets().Size(), 1);
		EXPECT_EQ(Result.GetAssets()[0].Id, FirstAsset.Id);
		EXPECT_EQ(Result.GetAssets()[0].Name, FirstAsset.Name);
	}
	{
		// search by asset names and types, both assets are of type Model
		csp::common::Array<csp::common::String> AssetNames = {FirstAsset.Name, SecondAsset.Name};

		csp::common::Array<csp::systems::EAssetType> AssetTypes = {csp::systems::EAssetType::VIDEO};
		auto [EmptyResult] = AWAIT_PRE(AssetSystem, GetAssetsByCriteria, RequestPredicate, {AssetCollection.Id}, nullptr, AssetNames, AssetTypes);
		EXPECT_EQ(EmptyResult.GetResultCode(), csp::services::EResultCode::Success);
		EXPECT_EQ(EmptyResult.GetAssets().Size(), 0);

		// next to Model append Video too
		AssetTypes	  = {csp::systems::EAssetType::VIDEO, csp::systems::EAssetType::MODEL};
		auto [Result] = AWAIT_PRE(AssetSystem, GetAssetsByCriteria, RequestPredicate, {AssetCollection.Id}, nullptr, AssetNames, AssetTypes);
		EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);
		EXPECT_EQ(Result.GetAssets().Size(), 2);

		bool FoundFirstAsset = false, FoundSecondAsset = false;

		const auto& RetrievedAssets = Result.GetAssets();

		for (int idx = 0; idx < RetrievedAssets.Size(); ++idx)
		{
			auto& CurrentAsset = RetrievedAssets[idx];

			if (CurrentAsset.Id == FirstAsset.Id)
			{
				FoundFirstAsset = true;
			}
			else if (CurrentAsset.Id == SecondAsset.Id)
			{
				FoundSecondAsset = true;
			}
		}

		EXPECT_EQ(FoundFirstAsset && FoundSecondAsset, true);
	}

	DeleteAsset(AssetSystem, AssetCollection, FirstAsset);
	DeleteAsset(AssetSystem, AssetCollection, SecondAsset);
	DeleteAssetCollection(AssetSystem, AssetCollection);

	DeleteSpace(SpaceSystem, Space.Id);

	LogOut(UserSystem);
}
#endif


// TODO: Fix this!
// Disabled as for some reason it is causing C# tests failures ONLY on TC. This will be investigated in a separate ticket
#if false
	#if RUN_ALL_UNIT_TESTS || RUN_ASSETSYSTEM_TESTS || RUN_ASSETSYSTEM_GETASSETS_FROM_MULTIPLE_ASSET_COLLECTIONS_TEST
CSP_PUBLIC_TEST(CSPEngine, AssetSystemTests, GetAssetsFromMultipleAssetCollectionsTest)
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

	char UniqueFirstAssetCollectionName[256];
	SPRINTF(UniqueFirstAssetCollectionName, "%s-%s", TestAssetCollectionName, GetUniqueHexString().c_str());

	char UniqueSecondAssetCollectionName[256];
	SPRINTF(UniqueSecondAssetCollectionName, "%s-%s", TestAssetCollectionName, GetUniqueHexString().c_str());

	char UniqueFirstAssetName[256];
	SPRINTF(UniqueFirstAssetName, "%s-%s", TestAssetName, GetUniqueHexString().c_str());

	char UniqueSecondAssetName[256];
	SPRINTF(UniqueSecondAssetName, "%s-%s", TestAssetName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	LogIn(UserSystem, UserId);

	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceType::Private, nullptr, nullptr, nullptr, Space);

	csp::systems::AssetCollection FirstAssetCollection;
	CreateAssetCollection(AssetSystem, Space.Id, nullptr, UniqueFirstAssetCollectionName, nullptr, nullptr, FirstAssetCollection);

	csp::systems::Asset FirstAsset;
	CreateAsset(AssetSystem, FirstAssetCollection, UniqueFirstAssetName, nullptr, FirstAsset);

	csp::systems::AssetCollection SecondAssetCollection;
	CreateAssetCollection(AssetSystem, Space.Id, nullptr, UniqueSecondAssetCollectionName, nullptr, nullptr, SecondAssetCollection);

	csp::systems::Asset SecondAsset;
	CreateAsset(AssetSystem, SecondAssetCollection, UniqueSecondAssetName, nullptr, SecondAsset);

	//{
	//	// try to search but don't specify any asset collection Ids, only add one Asset Id though
	//	csp::common::Array<csp::common::String> AssetIds = {FirstAsset.Id};
	//	csp::common::Array<csp::common::String> AssetCollIds;
	//	auto [Result] = AWAIT_PRE(AssetSystem, GetAssetsByCriteria, RequestPredicate, AssetCollIds, AssetIds, nullptr, nullptr);
	//	EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Failed);
	//}
	{
		// search by both asset collection Ids at the same time
		csp::common::Array<csp::common::String> AssetCollectionIds = {FirstAssetCollection.Id, SecondAssetCollection.Id};
		auto [Result] = AWAIT_PRE(AssetSystem, GetAssetsByCriteria, RequestPredicate, AssetCollectionIds, nullptr, nullptr, nullptr);
		EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);
		EXPECT_EQ(Result.GetAssets().Size(), 2);
		const auto& RetrievedAssets = Result.GetAssets();

		bool FoundFirstAsset = false, FoundSecondAsset = false;
		for (size_t idx = 0; idx < RetrievedAssets.Size(); ++idx)
		{
			auto& CurrentAsset = RetrievedAssets[idx];

			if (CurrentAsset.Id == FirstAsset.Id)
			{
				FoundFirstAsset = true;
			}
			else if (CurrentAsset.Id == SecondAsset.Id)
			{
				FoundSecondAsset = true;
			}
		}

		EXPECT_EQ(FoundFirstAsset && FoundSecondAsset, true);
	}
	{
		// search by both asset collection Ids and only one Asset Id
		csp::common::Array<csp::common::String> AssetCollectionIds = {FirstAssetCollection.Id, SecondAssetCollection.Id};
		csp::common::Array<csp::common::String> AssetIds			 = {SecondAsset.Id};
		auto [Result] = AWAIT_PRE(AssetSystem, GetAssetsByCriteria, RequestPredicate, AssetCollectionIds, AssetIds, nullptr, nullptr);
		EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);
		EXPECT_EQ(Result.GetAssets().Size(), 1);
		EXPECT_EQ(Result.GetAssets()[0].Id, SecondAsset.Id);
		EXPECT_EQ(Result.GetAssets()[0].Name, SecondAsset.Name);
	}

	DeleteAsset(AssetSystem, FirstAssetCollection, FirstAsset);
	DeleteAsset(AssetSystem, SecondAssetCollection, SecondAsset);
	DeleteAssetCollection(AssetSystem, FirstAssetCollection);
	DeleteAssetCollection(AssetSystem, SecondAssetCollection);

	DeleteSpace(SpaceSystem, Space);

	LogOut(UserSystem);
}
	#endif
#endif

#if RUN_ALL_UNIT_TESTS || RUN_ASSETSYSTEM_TESTS || RUN_ASSETSYSTEM_UPLOADASSET_AS_FILE_TEST
CSP_PUBLIC_TEST(CSPEngine, AssetSystemTests, UploadAssetAsFileTest)
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

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	// Create asset collection
	csp::systems::AssetCollection AssetCollection;
	CreateAssetCollection(AssetSystem, Space.Id, nullptr, UniqueAssetCollectionName, nullptr, nullptr, AssetCollection);

	// Create asset
	csp::systems::Asset Asset;
	CreateAsset(AssetSystem, AssetCollection, UniqueAssetName, nullptr, nullptr, Asset);
	auto FilePath = std::filesystem::absolute("assets/test.json");
	csp::systems::FileAssetDataSource Source;
	Source.FilePath							  = FilePath.u8string().c_str();
	const csp::common::String& FileNoMimeType = "";
	const csp::common::String& FileMimeType	  = "application/json";

	printf("Uploading asset data without mime type...\n");

	// Upload data
	auto [UploadNoMimeResult] = AWAIT_PRE(AssetSystem, UploadAssetData, RequestPredicateWithProgress, AssetCollection, Asset, Source);

	EXPECT_EQ(UploadNoMimeResult.GetResultCode(), csp::services::EResultCode::Success);

	Asset.Uri = UploadNoMimeResult.GetUri();

	printf("Getting asset to check for default mime type.\n");

	auto [AssetNoMimeResult] = AWAIT_PRE(AssetSystem, GetAssetById, RequestPredicate, AssetCollection.Id, Asset.Id);

	EXPECT_NE(AssetNoMimeResult.GetAsset().MimeType, FileNoMimeType);
	EXPECT_EQ(AssetNoMimeResult.GetAsset().MimeType, "application/octet-stream");

	// Set a mime type
	Source.SetMimeType(FileMimeType);

	printf("Uploading asset data with correct mime type...\n");

	// Upload data with MimeType
	auto [UploadResult] = AWAIT_PRE(AssetSystem, UploadAssetData, RequestPredicateWithProgress, AssetCollection, Asset, Source);

	EXPECT_EQ(UploadResult.GetResultCode(), csp::services::EResultCode::Success);

	EXPECT_EQ(UploadResult.GetXErrorCode(), "");

	Asset.Uri = UploadResult.GetUri();

	printf("Getting asset to check for correct mime type.\n");

	auto [AssetResult] = AWAIT_PRE(AssetSystem, GetAssetById, RequestPredicate, AssetCollection.Id, Asset.Id);

	EXPECT_EQ(AssetResult.GetAsset().MimeType, FileMimeType);

	printf("Downloading asset data...\n");

	// Get data
	auto [Result] = AWAIT_PRE(AssetSystem, DownloadAssetData, RequestPredicateWithProgress, Asset);

	EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);

	size_t DownloadedAssetDataSize = Result.GetDataLength();
	auto DownloadedAssetData	   = new uint8_t[DownloadedAssetDataSize];
	memcpy(DownloadedAssetData, Result.GetData(), DownloadedAssetDataSize);

	FILE* File		   = fopen(FilePath.string().c_str(), "rb");
	uintmax_t FileSize = std::filesystem::file_size(FilePath);
	auto* FileData	   = new unsigned char[FileSize];
	fread(FileData, FileSize, 1, File);
	fclose(File);

	EXPECT_EQ(DownloadedAssetDataSize, FileSize);
	EXPECT_EQ(memcmp(DownloadedAssetData, FileData, FileSize), 0);

	delete[] FileData;
	delete[] DownloadedAssetData;

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

#if RUN_ALL_UNIT_TESTS || RUN_ASSETSYSTEM_TESTS || RUN_ASSETSYSTEM_UPLOADASSET_AS_INCORRECT_FILE_TEST
CSP_PUBLIC_TEST(CSPEngine, AssetSystemTests, UploadAssetAsIncorrectFileTest)
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

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	// Create asset collection
	csp::systems::AssetCollection AssetCollection;
	CreateAssetCollection(AssetSystem, Space.Id, nullptr, UniqueAssetCollectionName, nullptr, nullptr, AssetCollection);

	// Create asset
	csp::systems::Asset Asset;
	CreateAsset(AssetSystem, AssetCollection, UniqueAssetName, nullptr, nullptr, Asset);
	auto FilePath = std::filesystem::absolute("assets/Incorrect_File.jpg");
	csp::systems::FileAssetDataSource Source;
	Source.FilePath							= FilePath.u8string().c_str();
	const csp::common::String& FileMimeType = "image/jpeg";

	// Upload data
	auto [Result] = AWAIT_PRE(AssetSystem, UploadAssetData, RequestPredicateWithProgress, AssetCollection, Asset, Source);

	EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Failed);

	EXPECT_EQ(Result.GetXErrorCode(), "assetdetail_invalidfilecontents");

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

#if RUN_ALL_UNIT_TESTS || RUN_ASSETSYSTEM_TESTS || RUN_ASSETSYSTEM_UPLOADASSET_AS_FILE_NOSPACE_TEST
CSP_PUBLIC_TEST(CSPEngine, AssetSystemTests, UploadAssetAsFileNoSpaceTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* AssetSystem	 = SystemsManager.GetAssetSystem();

	const char* TestAssetCollectionName = "OLY-UNITTEST-ASSETCOLLECTION-REWIND";
	const char* TestAssetName			= "OLY-UNITTEST-ASSET-REWIND";

	char UniqueAssetCollectionName[256];
	SPRINTF(UniqueAssetCollectionName, "%s-%s", TestAssetCollectionName, GetUniqueHexString().c_str());

	char UniqueAssetName[256];
	SPRINTF(UniqueAssetName, "%s-%s", TestAssetName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create asset collection
	csp::systems::AssetCollection AssetCollection;
	CreateAssetCollection(AssetSystem, nullptr, nullptr, UniqueAssetCollectionName, nullptr, nullptr, AssetCollection);

	// Create asset
	csp::systems::Asset Asset;
	CreateAsset(AssetSystem, AssetCollection, UniqueAssetName, nullptr, nullptr, Asset);
	auto FilePath = std::filesystem::absolute("assets/test.json");
	csp::systems::FileAssetDataSource Source;
	Source.FilePath							  = FilePath.u8string().c_str();
	const csp::common::String& FileNoMimeType = "";
	const csp::common::String& FileMimeType	  = "application/json";

	printf("Uploading asset data without mime type...\n");

	// Upload data
	auto [UploadNoMimeResult] = AWAIT_PRE(AssetSystem, UploadAssetData, RequestPredicateWithProgress, AssetCollection, Asset, Source);

	EXPECT_EQ(UploadNoMimeResult.GetResultCode(), csp::services::EResultCode::Success);

	Asset.Uri = UploadNoMimeResult.GetUri();

	printf("Getting asset to check for default mime type.\n");

	auto [AssetNoMimeResult] = AWAIT_PRE(AssetSystem, GetAssetById, RequestPredicate, AssetCollection.Id, Asset.Id);

	EXPECT_NE(AssetNoMimeResult.GetAsset().MimeType, FileNoMimeType);
	EXPECT_EQ(AssetNoMimeResult.GetAsset().MimeType, "application/octet-stream");

	// Set a mime type
	Source.SetMimeType(FileMimeType);

	printf("Uploading asset data with correct mime type...\n");

	// Upload data with MimeType
	auto [UploadResult] = AWAIT_PRE(AssetSystem, UploadAssetData, RequestPredicateWithProgress, AssetCollection, Asset, Source);

	EXPECT_EQ(UploadResult.GetResultCode(), csp::services::EResultCode::Success);

	Asset.Uri = UploadResult.GetUri();

	printf("Getting asset to check for correct mime type.\n");

	auto [AssetResult] = AWAIT_PRE(AssetSystem, GetAssetById, RequestPredicate, AssetCollection.Id, Asset.Id);

	EXPECT_EQ(AssetResult.GetAsset().MimeType, FileMimeType);

	printf("Downloading asset data...\n");

	// Get data
	auto [Result] = AWAIT_PRE(AssetSystem, DownloadAssetData, RequestPredicateWithProgress, Asset);

	EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);

	size_t DownloadedAssetDataSize = Result.GetDataLength();
	auto DownloadedAssetData	   = new uint8_t[DownloadedAssetDataSize];
	memcpy(DownloadedAssetData, Result.GetData(), DownloadedAssetDataSize);

	FILE* File		   = fopen(FilePath.string().c_str(), "rb");
	uintmax_t FileSize = std::filesystem::file_size(FilePath);
	auto* FileData	   = new unsigned char[FileSize];
	fread(FileData, FileSize, 1, File);
	fclose(File);

	EXPECT_EQ(DownloadedAssetDataSize, FileSize);
	EXPECT_EQ(memcmp(DownloadedAssetData, FileData, FileSize), 0);

	delete[] FileData;
	delete[] DownloadedAssetData;

	// Delete asset
	DeleteAsset(AssetSystem, AssetCollection, Asset);

	// Delete asset collection
	DeleteAssetCollection(AssetSystem, AssetCollection);

	// Log out
	LogOut(UserSystem);
}
#endif


#if RUN_ALL_UNIT_TESTS || RUN_ASSETSYSTEM_TESTS || RUN_ASSETSYSTEM_UPLOADASSET_AS_BUFFER_TEST
CSP_PUBLIC_TEST(CSPEngine, AssetSystemTests, UploadAssetAsBufferTest)
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

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	// Create asset collection
	csp::systems::AssetCollection AssetCollection;
	CreateAssetCollection(AssetSystem, Space.Id, nullptr, UniqueAssetCollectionName, nullptr, nullptr, AssetCollection);

	// Create asset
	csp::systems::Asset Asset;
	CreateAsset(AssetSystem, AssetCollection, UniqueAssetName, nullptr, nullptr, Asset);
	Asset.FileName = "test.json";

	auto UploadFilePath		 = std::filesystem::absolute("assets/test.json");
	FILE* UploadFile		 = fopen(UploadFilePath.string().c_str(), "rb");
	uintmax_t UploadFileSize = std::filesystem::file_size(UploadFilePath);
	auto* UploadFileData	 = new unsigned char[UploadFileSize];
	fread(UploadFileData, UploadFileSize, 1, UploadFile);
	fclose(UploadFile);

	csp::systems::BufferAssetDataSource BufferSource;
	BufferSource.Buffer		  = UploadFileData;
	BufferSource.BufferLength = UploadFileSize;

	BufferSource.SetMimeType("application/json");

	printf("Uploading asset data...\n");

	// Upload data
	UploadAssetData(AssetSystem, AssetCollection, Asset, BufferSource, Asset.Uri);

	printf("Downloading asset data...\n");

	// Get data
	auto [Result] = AWAIT_PRE(AssetSystem, DownloadAssetData, RequestPredicateWithProgress, Asset);

	EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);

	size_t DownloadedAssetDataSize = Result.GetDataLength();
	auto DownloadedAssetData	   = new uint8_t[DownloadedAssetDataSize];
	memcpy(DownloadedAssetData, Result.GetData(), DownloadedAssetDataSize);

	EXPECT_EQ(DownloadedAssetDataSize, UploadFileSize);
	EXPECT_EQ(memcmp(DownloadedAssetData, UploadFileData, UploadFileSize), 0);

	delete[] UploadFileData;
	delete[] DownloadedAssetData;

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


#if RUN_ALL_UNIT_TESTS || RUN_ASSETSYSTEM_TESTS || RUN_ASSETSYSTEM_UPDATEASSETDATA_AS_FILE_TEST
CSP_PUBLIC_TEST(CSPEngine, AssetSystemTests, UpdateAssetDataAsFileTest)
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

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

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

	printf("Uploading asset data...\n");

	csp::common::String Uri;

	UploadAssetData(AssetSystem, AssetCollection, Asset, Source, Uri);

	csp::systems::Asset UpdatedAsset;
	GetAssetById(AssetSystem, AssetCollection.Id, Asset.Id, UpdatedAsset);

	EXPECT_EQ(Asset.Id, UpdatedAsset.Id);

	// Replace data
	FilePath		= std::filesystem::absolute("assets/test2.json");
	Source.FilePath = FilePath.u8string().c_str();

	printf("Uploading new asset data...\n");

	csp::common::String Uri2;
	UploadAssetData(AssetSystem, AssetCollection, Asset, Source, Uri2);

	EXPECT_NE(Uri, Uri2);

	csp::systems::Asset UpdatedAsset2;
	GetAssetById(AssetSystem, AssetCollection.Id, Asset.Id, UpdatedAsset2);

	EXPECT_EQ(UpdatedAsset.Id, UpdatedAsset2.Id);

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


#if RUN_ALL_UNIT_TESTS || RUN_ASSETSYSTEM_TESTS || RUN_ASSETSYSTEM_UPDATEASSETDATA_AS_BUFFER_TEST
CSP_PUBLIC_TEST(CSPEngine, AssetSystemTests, UpdateAssetDataAsBufferTest)
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

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	// Create asset collection
	csp::systems::AssetCollection AssetCollection;
	CreateAssetCollection(AssetSystem, Space.Id, nullptr, UniqueAssetCollectionName, nullptr, nullptr, AssetCollection);

	// Create asset
	csp::systems::Asset Asset;
	CreateAsset(AssetSystem, AssetCollection, UniqueAssetName, nullptr, nullptr, Asset);
	auto& InitialAssetId = Asset.Id;

	// Upload data
	auto FilePath = std::filesystem::absolute("assets/test.json");
	csp::systems::FileAssetDataSource Source;
	Source.FilePath = FilePath.u8string().c_str();

	Source.SetMimeType("application/json");

	printf("Uploading asset data...\n");

	csp::common::String Uri;
	UploadAssetData(AssetSystem, AssetCollection, Asset, Source, Uri);

	// Replace data
	Asset.FileName = "test2.json";

	auto UpdateFilePath		 = std::filesystem::absolute("assets/test2.json");
	FILE* UpdateFile		 = fopen(UpdateFilePath.string().c_str(), "rb");
	uintmax_t UpdateFileSize = std::filesystem::file_size(UpdateFilePath);
	auto* UpdateFileData	 = new unsigned char[UpdateFileSize];
	fread(UpdateFileData, UpdateFileSize, 1, UpdateFile);
	fclose(UpdateFile);

	csp::systems::BufferAssetDataSource BufferSource;
	BufferSource.Buffer		  = UpdateFileData;
	BufferSource.BufferLength = UpdateFileSize;
	BufferSource.SetMimeType("application/json");

	printf("Uploading new asset data...\n");

	csp::common::String Uri2;
	UploadAssetData(AssetSystem, AssetCollection, Asset, BufferSource, Uri2);

	EXPECT_NE(Uri, Uri2);

	csp::systems::Asset UpdatedAsset;
	GetAssetById(AssetSystem, AssetCollection.Id, Asset.Id, UpdatedAsset);

	EXPECT_EQ(InitialAssetId, UpdatedAsset.Id);

	delete[] UpdateFileData;

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

#if RUN_ALL_UNIT_TESTS || RUN_ASSETSYSTEM_TESTS || RUN_ASSETSYSTEM_UPDATEASSETMETADATA_TEST
CSP_PUBLIC_TEST(CSPEngine, AssetSystemTests, UpdateAssetCollectionMetadataTest)
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

	char UniqueAssetName[256];
	SPRINTF(UniqueAssetName, "%s-%s", TestAssetName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	// Create asset collection
	csp::systems::AssetCollection AssetCollection;
	CreateAssetCollection(AssetSystem, Space.Id, nullptr, UniqueSpaceName, nullptr, nullptr, AssetCollection);

	csp::systems::AssetCollection IdAssetCollection;
	// Update MetaData
	csp::common::Map<csp::common::String, csp::common::String> MetaDataMapIn;
	csp::common::Map<csp::common::String, csp::common::String> MetaDataMapOut;
	MetaDataMapIn[UniqueSpaceName] = UniqueSpaceName;

	UpdateAssetCollectionMetadata(AssetSystem, AssetCollection, MetaDataMapIn, MetaDataMapOut);
	EXPECT_TRUE(MetaDataMapOut.HasKey(UniqueSpaceName));

	// Delete asset collection
	DeleteAssetCollection(AssetSystem, AssetCollection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif


#if RUN_ALL_UNIT_TESTS || RUN_ASSETSYSTEM_TESTS || RUN_ASSETSYSTEM_GETASSETDATASIZE_TEST
CSP_PUBLIC_TEST(CSPEngine, AssetSystemTests, GetAssetDataSizeTest)
{
	SetRandSeed();

	auto& SystemsManager = csp::systems::SystemsManager::Get();
	auto* UserSystem	 = SystemsManager.GetUserSystem();
	auto* AssetSystem	 = SystemsManager.GetAssetSystem();

	const char* TestAssetCollectionName = "OLY-UNITTEST-ASSETCOLLECTION";
	const char* TestAssetName			= "OLY-UNITTEST-ASSET";

	char UniqueAssetCollectionName[256];
	SPRINTF(UniqueAssetCollectionName, "%s-%s", TestAssetCollectionName, GetUniqueHexString().c_str());

	char UniqueAssetName[256];
	SPRINTF(UniqueAssetName, "%s-%s", TestAssetName, GetUniqueHexString().c_str());

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	// Create asset collection
	csp::systems::AssetCollection AssetCollection;
	CreateAssetCollection(AssetSystem, nullptr, nullptr, UniqueAssetCollectionName, nullptr, nullptr, AssetCollection);

	// Create asset
	csp::systems::Asset Asset;
	CreateAsset(AssetSystem, AssetCollection, UniqueAssetName, nullptr, nullptr, Asset);
	// Upload data
	Asset.FileName				  = "asimplejsonfile.json";
	csp::common::String AssetData = "{ \"some_value\": 42 }";
	csp::systems::BufferAssetDataSource Source;
	Source.Buffer		= (void*) AssetData.c_str();
	Source.BufferLength = AssetData.Length();
	Source.SetMimeType("application/json");

	printf("Uploading asset data...\n");

	csp::common::String Uri;
	UploadAssetData(AssetSystem, AssetCollection, Asset, Source, Uri);

	// Get updated asset
	csp::systems::Asset UpdatedAsset;
	GetAssetById(AssetSystem, AssetCollection.Id, Asset.Id, UpdatedAsset);

	EXPECT_EQ(Asset.Id, UpdatedAsset.Id);

	// Get asset data size
	{
		auto [Result] = AWAIT_PRE(AssetSystem, GetAssetDataSize, RequestPredicate, UpdatedAsset);

		EXPECT_EQ(Result.GetResultCode(), csp::services::EResultCode::Success);
		EXPECT_EQ(Result.GetValue(), AssetData.Length());
	}

	// Delete asset
	DeleteAsset(AssetSystem, AssetCollection, Asset);

	// Delete asset collection
	DeleteAssetCollection(AssetSystem, AssetCollection);

	// Log out
	LogOut(UserSystem);
}
#endif
#if RUN_ALL_UNIT_TESTS || RUN_ASSETSYSTEM_TESTS || RUN_ASSETSYSTEM_THIRDPARTYPACKAGEDASSETIDENTIFIER_TEST
CSP_PUBLIC_TEST(CSPEngine, AssetSystemTests, ThirdPartyPackagedAssetIdentifierTest)
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

	csp::common::String ThirdPartyPackagedAssetIdentifier	   = "OKO interoperable assets Test";
	csp::common::String ThirdPartyPackagedAssetIdentifierLocal = "OKO interoperable assets Test Local";

	csp::common::String UserId;

	// Log in
	LogIn(UserSystem, UserId);

	std::cout << UserId << "\n";

	// Create space
	csp::systems::Space Space;
	CreateSpace(SpaceSystem, UniqueSpaceName, TestSpaceDescription, csp::systems::SpaceAttributes::Private, nullptr, nullptr, nullptr, Space);

	// Create asset collection
	csp::systems::AssetCollection assetCollection;
	CreateAssetCollection(AssetSystem, Space.Id, nullptr, UniqueAssetCollectionName, nullptr, nullptr, assetCollection);

	// Create asset
	csp::systems::Asset Asset;
	CreateAsset(AssetSystem, assetCollection, UniqueAssetName, nullptr, nullptr, Asset);

	// Get assets
	csp::common::Array<csp::systems::Asset> Assets;
	GetAssetsInCollection(AssetSystem, assetCollection, Assets);

	EXPECT_EQ(Assets.Size(), 1);
	EXPECT_EQ(Assets[0].Name, UniqueAssetName);
	EXPECT_EQ(Assets[0].GetThirdPartyPackagedAssetIdentifier(), "");
	EXPECT_EQ(Assets[0].GetThirdPartyPlatformType(), csp::systems::EThirdPartyPlatform::NONE);
	// Delete asset
	DeleteAsset(AssetSystem, assetCollection, Asset);

	CreateAsset(AssetSystem, assetCollection, UniqueAssetName, ThirdPartyPackagedAssetIdentifier, csp::systems::EThirdPartyPlatform::UNITY, Asset);

	// Get assets
	GetAssetsInCollection(AssetSystem, assetCollection, Assets);

	EXPECT_EQ(Assets.Size(), 1);
	EXPECT_EQ(Assets[0].Name, UniqueAssetName);
	EXPECT_EQ(Assets[0].GetThirdPartyPackagedAssetIdentifier(), ThirdPartyPackagedAssetIdentifier);
	EXPECT_EQ(Assets[0].GetThirdPartyPlatformType(), csp::systems::EThirdPartyPlatform::UNITY);

	Assets[0].SetThirdPartyPackagedAssetIdentifier(ThirdPartyPackagedAssetIdentifierLocal);
	EXPECT_EQ(Assets[0].GetThirdPartyPackagedAssetIdentifier(), ThirdPartyPackagedAssetIdentifierLocal);
	// Delete asset
	DeleteAsset(AssetSystem, assetCollection, Asset);

	// Delete asset collection
	DeleteAssetCollection(AssetSystem, assetCollection);

	// Delete space
	DeleteSpace(SpaceSystem, Space.Id);

	// Log out
	LogOut(UserSystem);
}
#endif