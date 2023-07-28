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

#include "CSP/Systems/Script/ScriptSystem.h"

#include "CSP/CSPFoundation.h"
#include "CSP/Common/StringFormat.h"
#include "Debug/Logging.h"
#include "Memory/Memory.h"
#include "Memory/MemoryManager.h"
#include "Multiplayer/Script/ScriptHelpers.h"
#include "Systems/Script/ScriptContext.h"
#include "Systems/Script/ScriptRuntime.h"
#include "quickjspp.hpp"

#include <rapidjson/document.h>

// Enable optional 'os' and 'std' modules for file access etc
// #define SCRIPTS_INCLUDE_STD_LIBS

#if defined(SCRIPTS_INCLUDE_STD_LIBS)
	#include "quickjs-libc.h"
#endif

#include <map>
#include <sstream>


using namespace csp::common;


// For safer printfs
constexpr int MAX_SCRIPT_FUNCTION_LEN = 256;

constexpr auto ASSET_COLLECTION_NAME_PREFIX = "OKO_SCRIPTMODULENAMESPACE_";
constexpr auto LOOKUP_TABLE_METADATA_KEY	= "module_lookup_table";


// Template specializations for some custom csp types we want to use
template <> struct qjs::js_property_traits<String>
{
	static void set_property(JSContext* ctx, JSValue this_obj, String str, JSValue value)
	{
		int err = JS_SetPropertyStr(ctx, this_obj, str.c_str(), value);

		if (err < 0)
			throw exception {ctx};
	}

	static JSValue get_property(JSContext* ctx, JSValue this_obj, String str) noexcept
	{
		return JS_GetPropertyStr(ctx, this_obj, str.c_str());
	}
};

template <> struct qjs::js_traits<String>
{
	static String unwrap(JSContext* ctx, JSValueConst v)
	{
		size_t plen;
		const char* ptr = JS_ToCStringLen(ctx, &plen, v);

		if (!ptr)
			throw exception {ctx};

		return String(ptr, plen);
	}

	static JSValue wrap(JSContext* ctx, String str) noexcept
	{
		return JS_NewStringLen(ctx, str.c_str(), str.Length());
	}
};


namespace csp::systems
{

ScriptSystem::ScriptSystem() : TheScriptRuntime(nullptr)
{
}

ScriptSystem::~ScriptSystem()
{
	Shutdown();
}

void ScriptSystem::Initialise()
{
	if (TheScriptRuntime != nullptr)
	{
		FOUNDATION_LOG_ERROR_MSG("ScriptSystem::Initialise already called\n");
		return;
	}

	TheScriptRuntime = CSP_NEW ScriptRuntime(this);

#if defined(SCRIPTS_INCLUDE_STD_LIBS)
	js_std_init_handlers(TheScriptRuntime->Runtime->rt);
	JS_SetModuleLoaderFunc(TheScriptRuntime->Runtime->rt, nullptr, js_module_loader, nullptr);
	js_std_add_helpers(TheScriptRuntime->Context->ctx, 0, nullptr);

	js_init_module_std(TheScriptRuntime->Context->ctx, "std");
	js_init_module_os(TheScriptRuntime->Context->ctx, "os");
#else
// @todo WASM build may need a custom module loader
//	JS_SetModuleLoaderFunc(TheScriptRuntime->Runtime->rt, nullptr, js_module_loader, nullptr);
#endif

	// Define a module name alias to be used when importing a module by name in a script
	AddModuleUrlAlias(OLD_SCRIPT_NAMESPACE, SCRIPT_NAMESPACE);
}

void ScriptSystem::Shutdown()
{
	if (TheScriptRuntime != nullptr)
	{
		CSP_DELETE(TheScriptRuntime);
		TheScriptRuntime = nullptr;
	}
}

bool ScriptSystem::RunScript(int64_t ContextId, const String& ScriptText)
{
	// FOUNDATION_LOG_FORMAT(LogLevel::Verbose, "RunScript: %s\n", ScriptText.c_str());

	ScriptContext* TheScriptContext = TheScriptRuntime->GetContext(ContextId);
	if (TheScriptContext == nullptr)
	{
		return false;
	}

	qjs::Value Result = TheScriptContext->Context->eval(ScriptText.c_str(), "<eval>", JS_EVAL_TYPE_MODULE);
	bool HasErrors	  = Result.isException();
	return !HasErrors;
}

bool ScriptSystem::RunScriptFile(int64_t ContextId, const String& ScriptFilePath)
{
	FOUNDATION_LOG_FORMAT(LogLevel::Verbose, "RunScriptFile: %s\n", ScriptFilePath.c_str());

	ScriptContext* TheScriptContext = TheScriptRuntime->GetContext(ContextId);
	if (TheScriptContext == nullptr)
	{
		return false;
	}

	qjs::Value Result = TheScriptContext->Context->evalFile(ScriptFilePath.c_str(), JS_EVAL_TYPE_MODULE);
	bool HasErrors	  = Result.isException();
	return !HasErrors;
}

bool ScriptSystem::CreateContext(int64_t ContextId)
{
	return TheScriptRuntime->AddContext(ContextId);
}

bool ScriptSystem::DestroyContext(int64_t ContextId)
{
	return TheScriptRuntime->RemoveContext(ContextId);
}

bool ScriptSystem::BindContext(int64_t ContextId)
{
	return TheScriptRuntime->BindContext(ContextId);
}


bool ScriptSystem::ResetContext(int64_t ContextId)
{
	return TheScriptRuntime->ResetContext(ContextId);
}


bool ScriptSystem::ExistsInContext(int64_t ContextId, const String& ObjectName)
{
	return TheScriptRuntime->ExistsInContext(ContextId, ObjectName);
}


void* ScriptSystem::GetContext(int64_t ContextId)
{
	return (void*) TheScriptRuntime->GetContext(ContextId)->Context;
}

void* ScriptSystem::GetModule(int64_t ContextId, const String& ModuleName)
{
	ScriptContext* TheScriptContext = TheScriptRuntime->GetContext(ContextId);
	return (void*) TheScriptContext->GetModule(ModuleName)->Module;
}

void ScriptSystem::RegisterScriptBinding(IScriptBinding* ScriptBinding)
{
	TheScriptRuntime->RegisterScriptBinding(ScriptBinding);
}

void ScriptSystem::UnregisterScriptBinding(IScriptBinding* ScriptBinding)
{
	TheScriptRuntime->UnregisterScriptBinding(ScriptBinding);
}

void ScriptSystem::SetModuleSource(String ModuleUrl, String Source)
{
	TheScriptRuntime->SetModuleSource(ModuleUrl, Source);
}

void ScriptSystem::AddModuleUrlAlias(const String& ModuleUrl, const String& ModuleUrlAlias)
{
	TheScriptRuntime->AddModuleUrlAlias(ModuleUrl, ModuleUrlAlias);
}

void ScriptSystem::ClearModuleSource(String ModuleUrl)
{
	TheScriptRuntime->ClearModuleSource(ModuleUrl);
}

String ScriptSystem::GetModuleSource(String ModuleUrl)
{
	return TheScriptRuntime->GetModuleSource(ModuleUrl);
}

bool ScriptSystem::GetModuleUrlAlias(const String& ModuleUrl, String& OutModuleUrlAlias)
{
	return TheScriptRuntime->GetModuleUrlAlias(ModuleUrl, OutModuleUrlAlias);
}

size_t ScriptSystem::GetNumImportedModules(int64_t ContextId) const
{
	return TheScriptRuntime->GetContext(ContextId)->GetNumImportedModules();
}

const char* ScriptSystem::GetImportedModule(int64_t ContextId, size_t Index) const
{
	return TheScriptRuntime->GetContext(ContextId)->GetImportedModule(Index);
}


ScriptModuleCollection::ScriptModuleCollection() {};
ScriptModuleCollection::~ScriptModuleCollection() {};

const String& ScriptModuleCollection::GetId() const
{
	return Id;
}

const Map<String, String>& ScriptModuleCollection::GetLookupTable() const
{
	return LookupTable;
}

Map<String, String>& ScriptModuleCollection::GetMutableLookupTable() const
{
	return const_cast<decltype(LookupTable)&>(LookupTable);
}

ScriptModuleCollection& ScriptModuleCollectionResult::GetCollection()
{
	return Collection;
}

const ScriptModuleCollection& ScriptModuleCollectionResult::GetCollection() const
{
	return Collection;
}


ScriptModuleAsset::ScriptModuleAsset() {};

ScriptModuleAsset ::~ScriptModuleAsset() {};

const String& ScriptModuleAsset::GetId() const
{
	return Id;
}

const String& ScriptModuleAsset::GetModuleText() const
{
	return ModuleText;
}


ScriptModuleAsset& ScriptModuleAssetResult::GetModule()
{
	return Module;
}

const ScriptModuleAsset& ScriptModuleAssetResult::GetModule() const
{
	return Module;
}

void ScriptModuleAssetResult::SetResponseBody(const csp::common::String& Value)
{
	ResponseBody = Value;
}


void ScriptSystem::CreateScriptModuleCollection(const String& Namespace, const ScriptModuleCollectionResultCallback& Callback)
{
	String AssetCollectionName(ASSET_COLLECTION_NAME_PREFIX);
	AssetCollectionName.Append(Namespace);

	auto* AssetSystem = SystemsManager::Get().GetAssetSystem();

	AssetCollectionResultCallback CreateAssetCollectionCallback = [AssetSystem, Callback](const AssetCollectionResult& Result)
	{
		const auto& AssetCollection = Result.GetAssetCollection();

		ScriptModuleCollectionResult InternalResult(Result.GetResultCode(), Result.GetHttpResultCode());

		auto& Collection = InternalResult.GetCollection();
		Collection.Id	 = AssetCollection.Id;

		Callback(InternalResult);
	};

	// Create asset collection that represents script module collection
	AssetSystem->CreateAssetCollection(nullptr,
									   nullptr,
									   AssetCollectionName,
									   nullptr,
									   EAssetCollectionType::SCRIPT_MODULE_COLLECTION,
									   nullptr,
									   CreateAssetCollectionCallback);
}

void ScriptSystem::_GetScriptModuleCollectionCallback(ScriptModuleCollectionResultCallback Callback, const AssetCollectionResult& Result)
{
	const auto& AssetCollection = Result.GetAssetCollection();
	const auto& Metadata		= AssetCollection.GetMetadataImmutable();

	// TODO: Handle failures in GetLookupTableById
	ScriptModuleCollectionResult InternalResult(csp::services::EResultCode::Success, 400);

	// Grab and modify ScriptModuleCollection instance
	auto& Collection	   = InternalResult.GetCollection();
	Collection.Id		   = AssetCollection.Id;
	Collection.LookupTable = decltype(Collection.LookupTable)(Metadata); // Copy metadata map

	Callback(InternalResult);
}

void ScriptSystem::GetScriptModuleCollection(const String& Namespace, const ScriptModuleCollectionResultCallback& Callback)
{
	auto AssetCollectionName = String(ASSET_COLLECTION_NAME_PREFIX);
	AssetCollectionName.Append(Namespace);

	auto* AssetSystem = SystemsManager::Get().GetAssetSystem();

	// ScriptModuleCollection is stored as an AssetCollection on CHS
	AssetSystem->GetAssetCollectionByName(AssetCollectionName, std::bind(_GetScriptModuleCollectionCallback, Callback, std::placeholders::_1));
}

void ScriptSystem::GetScriptModuleCollectionById(const String& Id, const ScriptModuleCollectionResultCallback& Callback)
{
	auto* AssetSystem = SystemsManager::Get().GetAssetSystem();

	// ScriptModuleCollection is stored as an AssetCollection on CHS
	AssetSystem->GetAssetCollectionById(Id, std::bind(_GetScriptModuleCollectionCallback, Callback, std::placeholders::_1));
}


void ScriptSystem::DeleteScriptModuleCollection(const ScriptModuleCollection& Collection, const NullResultCallback& Callback)
{
	auto* AssetSystem = SystemsManager::Get().GetAssetSystem();

	AssetSystem->DeleteAssetCollection(Collection.GetId(), Callback);
}

void ScriptSystem::UpdateScriptModuleCollectionLookupTable(const ScriptModuleCollection& Collection,
														   const Map<String, String>& NewLookupTable,
														   const NullResultCallback& Callback)
{
	auto* AssetSystem = SystemsManager::Get().GetAssetSystem();

	// TODO: Remove this temporary AssetCollection when we switch to passing IDs instead of objects
	AssetCollection InternalAssetCollection;
	InternalAssetCollection.Id = Collection.Id;

	AssetCollectionResultCallback UpdateLookupTableCallback = [Callback](const AssetCollectionResult& Result)
	{
		NullResult InternalResult(Result.GetResultCode(), Result.GetHttpResultCode());

		Callback(InternalResult);
	};

	AssetSystem->UpdateAssetCollectionMetadata(InternalAssetCollection, NewLookupTable, UpdateLookupTableCallback);
}

void ScriptSystem::GetScriptModuleAsset(const ScriptModuleCollection& Collection,
										const csp::common::String& Name,
										const ScriptModuleAssetResultCallback& Callback)
{
	auto* AssetSystem		= SystemsManager::Get().GetAssetSystem();
	const auto& LookupTable = Collection.GetLookupTable();

	AssetResultCallback GetAssetCallback = [](const AssetResult& Result)
	{

	};

	if (LookupTable.HasKey(Name))
	{
		AssetSystem->GetAssetById(Collection.Id, LookupTable[Name], GetAssetCallback);
	}
	else
	{
		ScriptModuleCollectionResultCallback GetScriptModuleCollectionCallback
			= [Name, AssetSystem, GetAssetCallback, Callback](const ScriptModuleCollectionResult& Result)
		{
			if (Result.GetResultCode() == csp::services::EResultCode::Success)
			{
				const auto& Collection	= Result.GetCollection();
				const auto& LookupTable = Collection.GetLookupTable();

				if (LookupTable.HasKey(Name))
				{
					AssetSystem->GetAssetById(Collection.Id, LookupTable[Name], GetAssetCallback);
				}
				else
				{
					ScriptModuleAssetResult InternalResult(csp::services::EResultCode::Failed, Result.GetHttpResultCode());
					InternalResult.ResponseBody = "Module does not exist in namespace!";
					Callback(InternalResult);
				}
			}
			else if (Result.GetResultCode() == csp::services::EResultCode::Failed)
			{
				ScriptModuleAssetResult InternalResult(Result.GetResultCode(), Result.GetHttpResultCode());
				Callback(InternalResult);
			}
		};

		GetScriptModuleCollectionById(Collection.Id, GetScriptModuleCollectionCallback);
		// Check if module exists in lookup table
		// Return invalid if not, otherwise get script module asset
	}
}

void ScriptSystem::CreateScriptModuleAsset(const String& Namespace, const String& Name, const String& ModuleText, const NullResultCallback& Callback)
{
	auto* AssetSystem = SystemsManager::Get().GetAssetSystem();

	ScriptModuleCollectionResultCallback GetScriptModuleCollectionCallback
		= [this, AssetSystem, Name, ModuleText, Callback](const ScriptModuleCollectionResult& Result)
	{
		// TODO: Create some kind of mutex to prevent other clients from updating the lookup table while we are still using it here
		auto& Collection  = Result.GetCollection();
		auto& LookupTable = Collection.LookupTable;

		if (LookupTable.HasKey(Name))
		{
			NullResult InvalidNullResult = NullResult::Invalid();
			Callback(InvalidNullResult);
		}

		auto AssetName = StringFormat("%s_%s", Collection.GetId(), Name);

		csp::systems::AssetCollection InternalAssetCollection;
		InternalAssetCollection.Id = Collection.GetId();

		AssetResultCallback CreateAssetCallback
			= [this, AssetSystem, InternalAssetCollection, Collection, Name, ModuleText, LookupTable, Callback](const AssetResult& Result)
		{
			const auto& Asset = Result.GetAsset();

			BufferAssetDataSource AssetData;
			AssetData.SetMimeType("text/javascript");
			AssetData.Buffer	   = const_cast<char*>(ModuleText.c_str());
			AssetData.BufferLength = ModuleText.Length();

			UriResultCallback UploadAssetDataCallback = [this, Collection, Name, Asset, LookupTable, Callback](const UriResult& Result)
			{
				Map<String, String> NewLookupTable(LookupTable);
				NewLookupTable[Name] = Asset.Id;

				UpdateScriptModuleCollectionLookupTable(Collection, NewLookupTable, Callback);
			};

			AssetSystem->UploadAssetData(InternalAssetCollection, Asset, AssetData, UploadAssetDataCallback);
		};

		// TODO: Change this to instead take the AssetCollectionId - update method
		AssetSystem->CreateAsset(InternalAssetCollection, AssetName, nullptr, nullptr, EAssetType::SCRIPT_MODULE, CreateAssetCallback);
	};

	GetScriptModuleCollection(Namespace, GetScriptModuleCollectionCallback);
}

} // namespace csp::systems
