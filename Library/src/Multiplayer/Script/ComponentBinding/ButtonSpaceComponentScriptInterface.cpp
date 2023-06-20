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
#include "Multiplayer/Script/ComponentBinding/ButtonSpaceComponentScriptInterface.h"

#include "CSP/Multiplayer/Components/ButtonSpaceComponent.h"
#include "CSP/Multiplayer/SpaceEntity.h"
#include "Debug/Logging.h"

using namespace csp::systems;

namespace csp::multiplayer
{

ButtonSpaceComponentScriptInterface::ButtonSpaceComponentScriptInterface(ButtonSpaceComponent* InComponent) : ComponentScriptInterface(InComponent)
{
}

DEFINE_SCRIPT_PROPERTY_STRING(ButtonSpaceComponent, LabelText);
DEFINE_SCRIPT_PROPERTY_STRING(ButtonSpaceComponent, IconAssetId);
DEFINE_SCRIPT_PROPERTY_STRING(ButtonSpaceComponent, AssetCollectionId);

DEFINE_SCRIPT_PROPERTY_VEC3(ButtonSpaceComponent, Scale);
DEFINE_SCRIPT_PROPERTY_VEC3(ButtonSpaceComponent, Position);
DEFINE_SCRIPT_PROPERTY_VEC4(ButtonSpaceComponent, Rotation);

DEFINE_SCRIPT_PROPERTY_TYPE(ButtonSpaceComponent, bool, bool, IsVisible);
DEFINE_SCRIPT_PROPERTY_TYPE(ButtonSpaceComponent, bool, bool, IsEnabled);

} // namespace csp::multiplayer
