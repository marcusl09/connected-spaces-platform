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
#include "CSP/Multiplayer/Components/CustomSpaceComponent.h"

#include "Memory/Memory.h"
#include "Multiplayer/Script/ComponentBinding/CustomSpaceComponentScriptInterface.h"

#include <Debug/Logging.h>
#include <algorithm>
#include <functional>


constexpr const uint32_t CUSTOM_PROPERTY_LIST_KEY = 0;

namespace csp::multiplayer
{

CustomSpaceComponent::CustomSpaceComponent(SpaceEntity* Parent) : ComponentBase(ComponentType::Custom, Parent) //, NumProperties(2)
{
	Properties[static_cast<uint32_t>(CustomComponentPropertyKeys::ApplicationOrigin)] = "";

	SetScriptInterface(CSP_NEW CustomSpaceComponentScriptInterface(this));
}

const csp::common::String& CustomSpaceComponent::GetApplicationOrigin() const
{
	if (const auto& RepVal = GetProperty(static_cast<uint32_t>(CustomComponentPropertyKeys::ApplicationOrigin));
		RepVal.GetReplicatedValueType() == ReplicatedValueType::String)
	{
		return RepVal.GetString();
	}

	FOUNDATION_LOG_ERROR_MSG("Underlying ReplicatedValue not valid");
	return ReplicatedValue::GetDefaultString();
}

void CustomSpaceComponent::SetApplicationOrigin(const csp::common::String& Value)
{
	SetProperty(static_cast<uint32_t>(CustomComponentPropertyKeys::ApplicationOrigin), Value);
}

uint32_t CustomSpaceComponent::GetCustomPropertySubscriptionKey(const csp::common::String& Key)
{
	return static_cast<uint32_t>(std::hash<std::string> {}(Key.c_str()));
}

bool CustomSpaceComponent::HasCustomProperty(const csp::common::String& Key) const
{
	const uint32_t PropertyKey = static_cast<uint32_t>(std::hash<std::string> {}(Key.c_str()));

	return Properties.HasKey(PropertyKey);
}

const ReplicatedValue& CustomSpaceComponent::GetCustomProperty(const csp::common::String& Key) const
{
	const uint32_t PropertyKey = static_cast<uint32_t>(std::hash<std::string> {}(Key.c_str()));

	return GetProperty(PropertyKey);
}

void CustomSpaceComponent::SetCustomProperty(const csp::common::String& Key, const ReplicatedValue& Value)
{
	if (Value.GetReplicatedValueType() != ReplicatedValueType::InvalidType)
	{
		const uint32_t PropertyKey = static_cast<uint32_t>(std::hash<std::string> {}(Key.c_str()));
		if (!Properties.HasKey(PropertyKey))
		{
			AddKey(Key);
		}
		SetProperty(PropertyKey, Value);
	}
}

void CustomSpaceComponent::RemoveCustomProperty(const csp::common::String& Key)
{
	const uint32_t PropertyKey = static_cast<uint32_t>(std::hash<std::string> {}(Key.c_str()));

	if (Properties.HasKey(PropertyKey))
	{
		RemoveProperty(PropertyKey);
		RemoveKey(Key);
	}
}

csp::common::List<csp::common::String> CustomSpaceComponent::GetCustomPropertyKeys() const
{
	if (Properties.HasKey(CUSTOM_PROPERTY_LIST_KEY))
	{
		const auto& RepVal = GetProperty(CUSTOM_PROPERTY_LIST_KEY);

		if (RepVal.GetReplicatedValueType() == ReplicatedValueType::String && !RepVal.GetString().IsEmpty())
		{
			return GetProperty(CUSTOM_PROPERTY_LIST_KEY).GetString().Split(',');
		}
	}

	return csp::common::List<csp::common::String>();
}

int32_t CustomSpaceComponent::GetNumProperties() const
{
	if (Properties.HasKey(CUSTOM_PROPERTY_LIST_KEY))
	{
		return static_cast<uint32_t>(Properties.Size() - 1);
	}
	else
	{
		return static_cast<uint32_t>(Properties.Size());
	}
}

void CustomSpaceComponent::AddKey(const csp::common::String& Value)
{
	if (Properties.HasKey(CUSTOM_PROPERTY_LIST_KEY))
	{
		const auto& RepVal = GetProperty(CUSTOM_PROPERTY_LIST_KEY);

		if (RepVal.GetReplicatedValueType() != ReplicatedValueType::String)
		{
			return;
		}

		csp::common::String ReturnKeys = RepVal.GetString();

		if (!ReturnKeys.IsEmpty())
		{
			ReturnKeys = ReturnKeys + "," + Value;
			SetProperty(CUSTOM_PROPERTY_LIST_KEY, ReturnKeys);
		}
		else
		{
			SetProperty(CUSTOM_PROPERTY_LIST_KEY, Value);
		}
	}
	else
	{
		SetProperty(CUSTOM_PROPERTY_LIST_KEY, Value);
	}
}

void CustomSpaceComponent::RemoveKey(const csp::common::String& Key)
{
	const auto& RepVal = GetProperty(CUSTOM_PROPERTY_LIST_KEY);

	if (RepVal.GetReplicatedValueType() != ReplicatedValueType::String)
	{
		return;
	}

	const csp::common::String& CurrentKeys		   = RepVal.GetString();
	csp::common::List<csp::common::String> KeyList = CurrentKeys.Split(',');
	if (KeyList.Contains(Key))
	{
		csp::common::String ReturnKeys;
		KeyList.RemoveItem(Key);
		if (KeyList.Size() != 0)
		{
			for (int i = 0; i < KeyList.Size(); ++i)
			{
				if (i == 0)
				{
					ReturnKeys = KeyList[i];
				}
				else
				{
					ReturnKeys = ReturnKeys + "," + KeyList[i];
				}
			}
		}

		SetProperty(CUSTOM_PROPERTY_LIST_KEY, ReturnKeys);
	}
	else
	{
		FOUNDATION_LOG_ERROR_MSG("Key Not Found.");
	}
}

} // namespace csp::multiplayer
