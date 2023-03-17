#include "Memory.h"
#include <thread>
#include "offsets.h"
#include <iostream>
#include "defines.h"
//CSGOBUPROJECT
//Vec2 For Recoil Control System
struct Vector2
{
    float x = { }, y = { };
};
//need for chams
struct Color
{
    std::uint8_t r{ }, g{ }, b{ };
};

int main()
{
    auto mem = Memory{ "csgo.exe" };
    const auto client = mem.GetModuleAddress("client.dll");
    const auto engine = mem.GetModuleAddress("engine.dll");

    auto oldPunch = Vector2{ };

    std::cout << "Client Address: " << client;
    std::cout << "\nengine Address: " << engine;


    while (true)
    {
        const auto localTeam = mem.Read<std::uintptr_t>(client + offsets::m_iTeamNum);
        const auto localPlayer = mem.Read<std::uintptr_t>(client + offsets::dwLocalPlayer);
        const auto glowObjectManager = mem.Read<std::uintptr_t>(client + offsets::dwGlowObjectManager);
        const auto& shotsFired = mem.Read<std::int32_t>(localPlayer + offsets::m_iShotsFired);
        const auto& localHealth = mem.Read<std::int32_t>(localPlayer + offsets::m_iHealth);
        //Recoil Control
        if (shotsFired)
        {
            const auto& clientState = mem.Read<std::uintptr_t>(engine + offsets::dwClientState);
            const auto& viewAngles = mem.Read<Vector2>(clientState + offsets::dwClientState_ViewAngles);
            const auto& aimPunch = mem.Read<Vector2>(localPlayer + offsets::m_aimPunchAngle);

            auto newAngles = Vector2
            {
                viewAngles.x + oldPunch.x - aimPunch.x * 2.f,
                viewAngles.y + oldPunch.y - aimPunch.y * 2.f,
            };

            if (newAngles.x > 89.f)
                newAngles.x = 89.f;
            if (newAngles.x < -89.f)
                newAngles.x = -89.f;

            while (newAngles.y > 180.f)
                newAngles.y -= 360.f;
            while (newAngles.y > -180.f)
                newAngles.y -= 360.f;

            mem.Write<Vector2>(clientState + offsets::dwClientState_ViewAngles, newAngles);

            oldPunch.x = aimPunch.x * 2.f;
            oldPunch.y = aimPunch.y * 2.f;
        }
        else
        {
            oldPunch.x = oldPunch.y = 0.f;
        }

        //glow
        for (auto i = 0; i < 64; ++i)
        {
            const auto entity = mem.Read<std::uintptr_t>(client + offsets::dwEntityList + i * 0x10);
            if (mem.Read<std::uintptr_t>(entity + offsets::m_iTeamNum) == mem.Read<std::uintptr_t>(localPlayer + offsets::m_iTeamNum))
                continue;

            const auto glowIndex = mem.Read<std::int32_t>(entity + offsets::m_iGlowIndex);

            mem.Write<float>(glowObjectManager + (glowIndex * 0x38) + 0x8, 0.f); // red
            mem.Write<float>(glowObjectManager + (glowIndex * 0x38) + 0xC, 0.f); // green
            mem.Write<float>(glowObjectManager + (glowIndex * 0x38) + 0x10, 1.f); // blue
            mem.Write<float>(glowObjectManager + (glowIndex * 0x38) + 0x14, 0.6f); // Brightness

            mem.Write<bool>(glowObjectManager + (glowIndex * 0x38) + 0x27, true);
            mem.Write<bool>(glowObjectManager + (glowIndex * 0x38) + 0x28, true);
        }
        //bhop
        if (localPlayer)
        {
            const auto onGround = mem.Read<bool>(localPlayer + offsets::m_fFlags);

            if (GetAsyncKeyState(VK_SPACE) && onGround & (1 << 0))
            mem.Write<BYTE>(client + offsets::dwForceJump, 6);
        }

        for (auto i = 0; i < 64; ++i)
        {
            const auto entity = mem.Read<std::uintptr_t>(client + offsets::dwEntityList + i * 0x10);
            if (mem.Read<std::uintptr_t>(entity + offsets::m_iTeamNum) == localTeam)
                continue;
            mem.Write<bool>(entity + offsets::m_bSpotted, true);
        }

        // Chams
        constexpr const auto teamColor = Color{ 0,0,255 };
        constexpr const auto enemyColor = Color{ 255,0,0 };

        for (auto i = 1; i <= 32; ++i)
        {
            const auto entity = mem.Read<std::uintptr_t>(client + offsets::dwEntityList + i * 0x10);
            if (mem.Read<std::int32_t>(entity + offsets::m_iTeamNum) == localTeam)
            {
                mem.Write<Color>(entity + offsets::m_clrRender, teamColor);
            }
            else
            {
                mem.Write<Color>(entity + offsets::m_clrRender, enemyColor);
            }
            float brightness = 25.0f;
            const auto _this = static_cast<std::uintptr_t>(engine + offsets::model_ambient_min - 0x2c);
            mem.Write<std::int32_t>(engine + offsets::model_ambient_min, *reinterpret_cast<std::uintptr_t*>(&brightness) ^ _this);
        }


        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        // Trigger bot
        if (GetAsyncKeyState(VK_SHIFT))
            continue;

        if (!localHealth)
            continue;

        const auto& crosshairID = mem.Read<std::int32_t>(localPlayer + offsets::m_iCrosshairId);

        if (!crosshairID || crosshairID > 64)
            continue;
            
        const auto& player = mem.Read<std::uintptr_t>(client + offsets::dwEntityList + (crosshairID - 1) * 0x10);

        if (!mem.Read<std::int32_t>(player + offsets::m_iHealth))
            continue;
        if (mem.Read<std::int32_t>(player + offsets::m_iTeamNum) ==
            mem.Read<std::int32_t>(localPlayer + offsets::m_iTeamNum))
            continue;

        mem.Write<std::uintptr_t>(client + offsets::dwForceAttack, 6);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        mem.Write<std::uintptr_t>(client + offsets::dwForceAttack, 4);
    }
    return 0;
}
