#pragma once

#include <iostream>
#include <memory>
#include <utility>
#include <algorithm>
#include <functional>

#include <string>
#include <sstream>
#include <array>
#include <vector>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>

#include "Server/core/Log.h"


//for temporary define 
using TimeStep = float;
enum class ComponentID : uint16_t {
    None = 0,
    TransformComponent = 1,
    Renderable = 2,
    TagComponent = 3,
    IDComponent = 4,
    NativeScriptComponent = 5,
    ScriptComponent = 6,
    CameraComponent = 7,
    Rigidbody2DComponent = 8,
    BoxCollider2DComponent = 9,
    TextComponent = 10
};
using vec3 = float[3];
using UUID = uint64_t;
using RoomID = uint8_t;
using PlayerID = uint8_t;