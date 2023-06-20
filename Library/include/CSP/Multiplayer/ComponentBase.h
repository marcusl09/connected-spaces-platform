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
#pragma once

#include "CSP/CSPCommon.h"
#include "CSP/Common/Map.h"
#include "CSP/Common/String.h"
#include "CSP/Multiplayer/ReplicatedValue.h"

#include <functional>

CSP_START_IGNORE
#ifdef CSP_TESTS
class CSPEngine_SerialisationTests_SpaceEntityUserSignalRSerialisationTest_Test;
class CSPEngine_SerialisationTests_SpaceEntityUserSignalRDeserialisationTest_Test;
class CSPEngine_SerialisationTests_SpaceEntityObjectSignalRSerialisationTest_Test;
class CSPEngine_SerialisationTests_SpaceEntityObjectSignalRDeserialisationTest_Test;
#endif
CSP_END_IGNORE

namespace csp::multiplayer
{

class SpaceEntity;
class ComponentScriptInterface;

/// @brief Represents the type of component.
///
/// Values with _DEPRECATED appended to their name should not be used. They are retained only for backwards compatibility.
enum class ComponentType
{
	Invalid,
	Core,
	UIController_DEPRECATED,
	StaticModel,
	AnimatedModel,
	MediaSurface_DEPRECATED,
	VideoPlayer,
	ImageSequencer_DEPRECATED,
	ExternalLink,
	AvatarData,
	Light,
	Button,
	Image,
	ScriptData,
	Custom,
	Conversation,
	Portal,
	Audio,
	Spline,
	Collision,
	Reflection,
	Fog
};

/// @brief The base class for all components, provides mechanisms for dirtying properties and subscribing to events on property changes.
class CSP_API ComponentBase
{
	CSP_START_IGNORE
	/** @cond DO_NOT_DOCUMENT */
	friend class SpaceEntity;
	friend class EntityScriptInterface;
#ifdef CSP_TESTS
	friend class ::CSPEngine_SerialisationTests_SpaceEntityUserSignalRSerialisationTest_Test;
	friend class ::CSPEngine_SerialisationTests_SpaceEntityUserSignalRDeserialisationTest_Test;
	friend class ::CSPEngine_SerialisationTests_SpaceEntityObjectSignalRSerialisationTest_Test;
	friend class ::CSPEngine_SerialisationTests_SpaceEntityObjectSignalRDeserialisationTest_Test;
#endif
	/** @endcond */
	CSP_END_IGNORE

public:
	/// @brief A callback that can be registered to be called when an action of a given name is invoked. Contains a pointer
	/// to the component, the action name as a string and parameters as a string.
	typedef std::function<void(ComponentBase*, const csp::common::String&, const csp::common::String&)> EntityActionHandler;

	/// @brief Virtual destructor for the component.
	virtual ~ComponentBase();

	/// @brief Get the ID for this component.
	///
	/// This is set when calling SpaceEntity::AddComponent and is autogenerated with the intention of being unique
	/// within the context of the entity it is attached to.
	///
	/// @return The ID.
	uint16_t GetId();

	/// @brief Get the ComponentType of the component.
	/// @return The type of the component as an enum.
	ComponentType GetComponentType();

	/// @brief Get a map of the replicated values defined for this component.
	///
	/// The index of the map represents a unique index for the property,
	/// intended to be defined in the inherited component as an enum of available properties keys.
	///
	/// @return A map of the replicated values, keyed by their unique key.
	const csp::common::Map<uint32_t, ReplicatedValue>* GetProperties();

	/// @brief Get the parent SpaceEntity for this component. Components can only attach to one parent.
	/// @return A pointer to the parent SpaceEntity.
	SpaceEntity* GetParent();

	/// @brief Part of the scripting interface, allows you to subscribe to a property change and assign a script message to execute when activated.
	/// @param PropertyKey uint32_t : The key of the property to subscribe to.
	/// @param Message csp::common::String : The message to be sent to the script.
	CSP_NO_EXPORT void SubscribeToPropertyChange(uint32_t PropertyKey, csp::common::String Message);

	/// @brief Register an action handler callback to be called when the given action is invoked.
	/// @param InAction csp::common::String : The identifying name of the action.
	/// @param ActionHandler EntityActionHandler : Callback to be called when the action is invoked, contains
	/// a pointer to this component, the name of the action and a string of parameters for the action.
	void RegisterActionHandler(const csp::common::String& InAction, EntityActionHandler ActionHandler);

	/// @brief Removes the action handler callback for the given action.
	/// @param InAction csp::common::String : The identifying name of the action.
	void UnregisterActionHandler(const csp::common::String& InAction);

	/// @brief Calls the registered action handler callback for the given action and passes the given parameters.
	/// @param InAction csp::common::String : The identifying name of the action.
	/// @param InActionParams csp::common::String : Parameters for the action that will be passed to the action handler callback.
	void InvokeAction(const csp::common::String& InAction, const csp::common::String& InActionParams);

protected:
	ComponentBase();
	ComponentBase(ComponentType Type, SpaceEntity* Parent);

	const ReplicatedValue& GetProperty(uint32_t Key) const;
	void SetProperty(uint32_t Key, const ReplicatedValue& Value);
	void RemoveProperty(uint32_t Key);
	void SetProperties(const csp::common::Map<uint32_t, ReplicatedValue>& Value);

	virtual void SetPropertyFromPatch(uint32_t Key, const ReplicatedValue& Value);

	virtual void OnRemove();

	CSP_START_IGNORE
	void SetScriptInterface(ComponentScriptInterface* ScriptInterface);
	ComponentScriptInterface* GetScriptInterface();
	CSP_END_IGNORE

	SpaceEntity* Parent;
	uint16_t Id;
	ComponentType Type;
	csp::common::Map<uint32_t, ReplicatedValue> Properties;
	csp::common::Map<uint32_t, ReplicatedValue> DirtyProperties;

	ComponentScriptInterface* ScriptInterface;

	csp::common::Map<csp::common::String, EntityActionHandler> ActionMap;
};

} // namespace csp::multiplayer
