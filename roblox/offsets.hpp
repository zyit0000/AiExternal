#pragma once
// Stub to satisfy dumper.hpp
namespace roblox {
    struct Vector3 {
        float x, y, z;
        float magnitude() const { return sqrt(x*x + y*y + z*z); }
    };
    struct CFrame {
        float r0, r1, r2, r10, r11, r12, r20, r21, r22;
        Vector3 position;
    };
}

namespace offsets {
    namespace Instance { inline uintptr_t INSTANCE_SELF = 0x8; inline uintptr_t INSTANCE_NAME = 0x48; }
    namespace Camera { inline uintptr_t CAMERA_FIELDOFVIEW = 0; inline uintptr_t CAMERA_CFRAME = 0; inline uintptr_t CAMERA_CAMERASUBJECT = 0; }
    namespace Player { inline uintptr_t PLAYER_CHARACTER = 0; inline uintptr_t PLAYER_DISPLAYNAME = 0; inline uintptr_t PLAYER_TEAM = 0; inline uintptr_t PLAYER_USERID = 0; inline uintptr_t PLAYER_ACCOUNTAGE = 0; }
    namespace Humanoid { inline uintptr_t HUMANOID_HEALTH = 0; inline uintptr_t HUMANOID_MAXHEALTH = 0; inline uintptr_t HUMANOID_WALKSPEED = 0; inline uintptr_t HUMANOID_HIPHEIGHT = 0; inline uintptr_t HUMANOID_JUMPPOWER = 0; inline uintptr_t HUMANOID_JUMPHEIGHT = 0; inline uintptr_t HUMANOID_DISPLAYNAME = 0; inline uintptr_t HUMANOID_SEATPART = 0; }
    namespace BasePart { inline uintptr_t BASEPART_PROPERTIES = 0; inline uintptr_t BASEPART_COLOR = 0; inline uintptr_t BASEPART_TRANSPARENCY = 0; }
    namespace Primitive { inline uintptr_t BASEPART_PROPS_CFRAME = 0; inline uintptr_t BASEPART_PROPS_POSITION = 0; inline uintptr_t BASEPART_PROPS_VELOCITY = 0; inline uintptr_t BASEPART_PROPS_ROTVELOCITY = 0; inline uintptr_t BASEPART_PROPS_RECEIVEAGE = 0; inline uintptr_t BASEPART_PROPS_SIZE = 0; inline uintptr_t BASEPART_PROPS_CANCOLLIDE = 0; }
    namespace Players { inline uintptr_t PLAYERS_LOCALPLAYER = 0; inline uintptr_t PLAYERS_MAXPLAYERS = 0; }
    namespace Workspace { inline uintptr_t WORKSPACE_CURRENTCAMERA = 0; }
}
