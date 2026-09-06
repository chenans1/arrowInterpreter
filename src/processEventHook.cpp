#include "PCH.h"
#include "processEventHook.h"
#include "payload.h"
#include <cmath>
#include <numbers>
#include <random>

using namespace SKSE;
using namespace SKSE::log;
using namespace std::literals;

namespace arrow {
    static float randomFloat(float minimum, float maximum) {
        static thread_local std::mt19937 generator{std::random_device{}()};
        std::uniform_real_distribution<float> distribution{minimum, maximum};
        return distribution(generator);
    }

    //doing npcs and pcs
    void ProcessEventHook::Install() { 
        log::info("[processEventHook] Install ProcessEvent() Hook");

        REL::Relocation<std::uintptr_t> vtblNPC{RE::VTABLE_Character[2]};
        REL::Relocation<std::uintptr_t> vtblPC{RE::VTABLE_PlayerCharacter[2]};

        _originalNPC = vtblNPC.write_vfunc(0x1, ProcessEvent_NPC);
        _originalPC = vtblPC.write_vfunc(0x1, ProcessEvent_PC);

        log::info("[processEventHook] ...ProcessEvent hook installed");
    }

    //running this install separated - this way I can probably chain hook TDM and do it right after
    void ProcessEventHook::InstallProjectileHook() {
        log::info("[processEventHook] Installing Projectile Hook");
        auto& trampoline = SKSE::GetTrampoline();

        //hook locations derived from TDM, doing a hook after TDM to avoid dll alphabetical sorting shenanigans
        REL::Relocation<std::uintptr_t> hook{RELOCATION_ID(43030, 44222)};

        _InitProjectile = trampoline.write_call<5>(hook.address() + REL::Relocate(0x3B8, 0x78A), InitProjectile);
        log::info("[processEventHook] ...Projectile hook installed");
    }

    static bool releaseArrowOffset(RE::TESAmmo* ammo, RE::TESObjectWEAP* weapon, RE::Actor* actor, float damageMult = 1.0f, float offset = 0.0f) {
        if (!ammo || !weapon || !actor) {
            log::info("[arrowInterpreter] invalid params for releaseArrow()");
            return false;
        }

        auto* currentProcess = actor->GetActorRuntimeData().currentProcess;
        if (!currentProcess) {
            log::warn("[arrowInterpreter] Actor {:08X} has no current process", actor->GetFormID());
            return false;
        }

        //fetches the weapon node, not sure if this is a good idea.
        const auto& biped = actor->GetBiped2();
        RE::NiAVObject* weapon3D = nullptr;

        for (std::size_t i = 0; i < RE::BIPED_OBJECTS::kTotal; ++i) {
            auto& object = biped->objects[i];
            if (object.item == weapon && object.partClone) {
                weapon3D = object.partClone.get();
                break;
            }
        }

        RE::NiPoint3 origin;
        RE::Projectile::ProjectileRot rotation{};

        if (weapon3D) {
            origin = weapon3D->world.translate;
            //rotation.x = actor->GetAimAngle();
            //rotation.z = actor->GetAimHeading();
        } else {
            origin = actor->GetPosition();
            origin.z += 96.0f;
            //rotation.x = actor->GetAimAngle();
            //rotation.z = actor->GetAimHeading();
            log::warn("[arrowInterpreter] No weapon/fire node; using actor fallback");
        }

        rotation.x = actor->GetAimAngle();
        rotation.z = actor->GetAimHeading();
        RE::ProjectileHandle handle;
        RE::Projectile::LaunchData launchData(actor, origin, rotation, ammo, weapon);
        launchData.autoAim = false;
        //launchData.desiredTarget = nullptr;
        RE::Projectile::Launch(&handle, launchData);
        auto projectile = handle.get();
        if (!projectile) {
            log::error("[arrowInterpreter] Failed to launch arrow for actor {:08X}", actor->GetFormID());
            return false;
        }
        auto& projectileData = projectile->GetProjectileRuntimeData();
        if (projectileData.power > 0.0f) {
            projectileData.weaponDamage /= projectileData.power;
            projectileData.power = 1.0f;
            projectileData.weaponDamage *= damageMult;
        }
        //log::info("offset deg={}, offset rad={}",offset, RE::deg_to_rad(offset));

        if (offset != 0.0f) {
            ProcessEventHook::AddPendingArrow(projectile.get(), {.offset = RE::deg_to_rad(offset)});
        }
        //auto point = projectile->GetAngle();
        //log::info("[arrowInterpreter] Original Angle=({}, {}, {})", point.x, point.y, point.z);
        //point.z -= offset;
        //projectile->SetAngle(point);
        //log::info("[arrowInterpreter] Original Angle=({}, {}, {})", projectile->GetAngle().x, projectile->GetAngle().y, projectile->GetAngle().z);
        return true;
    }

    //releases arrow rain in a circle, hopefully. Idk lol
    static bool ReleaseArrowRain(RE::TESAmmo* ammo, RE::TESObjectWEAP* weapon, RE::Actor* actor, ArrowData a_data, float damageMult=1.0f) {
        if (!ammo || !weapon || !actor) {
            log::info("[arrowInterpreter] invalid params for releaseArrow()");
            return false;
        }

        auto* currentProcess = actor->GetActorRuntimeData().currentProcess;
        if (!currentProcess) {
            log::warn("[arrowInterpreter] Actor {:08X} has no current process", actor->GetFormID());
            return false;
        }
        // fetches the weapon node, not sure if this is a good idea.
        const auto& biped = actor->GetBiped2();
        RE::NiAVObject* weapon3D = nullptr;

        for (std::size_t i = 0; i < RE::BIPED_OBJECTS::kTotal; ++i) {
            auto& object = biped->objects[i];
            if (object.item == weapon && object.partClone) {
                weapon3D = object.partClone.get();
                break;
            }
        }
        RE::NiPoint3 origin;
        RE::Projectile::ProjectileRot rotation{};

        if (weapon3D) {
            origin = weapon3D->world.translate;
        } else {
            origin = actor->GetPosition();
            origin.z += 96.0f;
            log::warn("[arrowInterpreter] No weapon/fire node; using actor fallback");
        }

        rotation.x = actor->GetAimAngle(); 
        rotation.z = actor->GetAimHeading();

        RE::ProjectileHandle handle;
        RE::Projectile::LaunchData launchData(actor, origin, rotation, ammo, weapon);
        launchData.autoAim = false;
        launchData.desiredTarget = nullptr;
        RE::Projectile::Launch(&handle, launchData);
        auto projectile = handle.get();
        if (!projectile) {
            log::error("[arrowInterpreter] Failed to launch arrowRain for actor {:08X}", actor->GetFormID());
            return false;
        }
        auto& projectileData = projectile->GetProjectileRuntimeData();
        if (projectileData.power > 0.0f) {
            projectileData.weaponDamage /= projectileData.power;
            projectileData.power = 1.0f;
            projectileData.weaponDamage *= damageMult;
            //projectileData.scale *= 2.0f;
        }
        
        if (a_data.radius > 0.0f) {
            const float angle = randomFloat(0.0f, 2.0f * 3.1415926f);
            const float offset_radius = a_data.radius * std::sqrt(randomFloat(0.0f, 1.0f));

            const float forwardOffset = offset_radius * std::cosf(angle) * 1.2f;
            const float lateralOffset = offset_radius * std::sinf(angle) * 0.42f;
            ProcessEventHook::AddPendingArrow(
                projectile.get(),
                {
                    .isArrowRain = true,
                    .targetForward = a_data.targetForward + forwardOffset,
                    .targetLateral = lateralOffset,
                    .apex = a_data.apex,
                    .duration = a_data.duration
                });
        } else {
            ProcessEventHook::AddPendingArrow(projectile.get(), a_data);
        }
        return true;
    }

    //checks if it's our tag
    static void HandleEvent(RE::BSAnimationGraphEvent* a_event) {
        if (!a_event || !a_event->holder || !a_event->tag.data()) return;
        auto* holder = const_cast<RE::TESObjectREFR*>(a_event->holder);
        if (!holder) return;
        auto* actor = holder ? holder->As<RE::Actor>() : nullptr;
        if (!actor) return;

        const auto& tag = a_event->tag;
        const auto& payload = a_event->payload;
        if (tag != "arrowInterpreter"sv && tag != "ArrowRain"sv) {
            return;
        }
        //process payload
        //check for actor equipped items - need equipped bow and arrows
        auto* equippedForm = actor->GetEquippedObject(false);
        auto* bow = equippedForm ? equippedForm->As<RE::TESObjectWEAP>() : nullptr;

        if (!bow || !bow->IsBow()) {
            log::info("[arrowInterpreter] Actor {:08X} has no bow equipped", actor->GetFormID());
            return;
        }

        auto* ammo = actor->GetCurrentAmmo();
        if (!ammo) {
            log::info("[arrowInterpreter] Actor {:08X} has no ammunition equipped", actor->GetFormID());
            return;
        }
        //tag is arrow rain
        if (tag == "ArrowRain"sv) {
            // log::info("[arrowInterpreter] Actor {:08X} released arrow rain", actor->GetFormID());
            ReleaseArrowRain(ammo, bow, actor, {.isArrowRain=true,.radius = 0.0f}, 0.33f);
            for (std::uint32_t i = 0; i < 10; ++i) {
                ReleaseArrowRain(ammo, bow, actor, {.isArrowRain=true}, 0.33f);
            }
            return;
        }
        const auto params = process(payload);
        //log::info("[releaseArrow] dmg={} count={} spread={} consume={}", params.damageMult, params.count, params.spread,
        //          params.consume);
        if (params.consume > 0) {
            const std::int32_t ammoCount = actor->GetInventoryItemCount(ammo);

            if (ammoCount < static_cast<std::int32_t>(params.consume)) {
                log::info("[releaseArrow] Actor {:08X}: insufficient ammo ({}/{})", actor->GetFormID(), ammoCount, params.consume);
                return;
            }
        }

        if (params.count == 1) {
            if (!releaseArrowOffset(ammo, bow, actor, params.damageMult, 0.0f)) {
                return;
            }
        } else {
            //just doing even distribution along the spread angle
            //eg. 30 deg, 3 arrows => (-15, 0, 15)
            const float step = params.spread / static_cast<float>(params.count - 1);
            const float start = -(params.spread * 0.5f);
            for (std::uint32_t i = 0; i < params.count; ++i) {
                const float offset = start + step * static_cast<float>(i);
                //log::info("[releaseArrow]: fired w/ offset={}", offset);
                releaseArrowOffset(ammo, bow, actor, params.damageMult, offset);
            }
        }

        if (params.consume > 0) {
            actor->UseAmmo(params.consume);
        }
    }

    RE::BSEventNotifyControl ProcessEventHook::ProcessEvent_NPC(
        RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_sink, 
        RE::BSAnimationGraphEvent* a_event,
        RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource) 
    {
        HandleEvent(a_event);
        return _originalNPC(a_sink, a_event, a_eventSource);
    }

    RE::BSEventNotifyControl ProcessEventHook::ProcessEvent_PC(
        RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_sink,
        RE::BSAnimationGraphEvent* a_event,
        RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource) 
    {
        HandleEvent(a_event);
        return _originalPC(a_sink, a_event, a_eventSource);
    }

    void ProcessEventHook::AddPendingArrow(const RE::Projectile* a_projectile, ArrowData a_data) {
        std::scoped_lock lock(pendingArrowsMutex);
        pendingArrows.emplace(a_projectile, a_data);
        //log::info("[arrowInterpreter] Stored Arrow");
    }

    static bool calculateNewVelocity(
        RE::Projectile* projectile,
        float targetForward,
        float targetLateral,
        float apexHeight,
        float flightTime) {
        if (!projectile || targetForward <= 0.0f || apexHeight <= 0.0f || flightTime <= 0.0f) {
            return false;
        }
        //matching the values reverse engineered by smoothcam
        constexpr float havokToGameUnits = 59.0f;
        float worldGravityZ = -9.8f;
        if (auto* cell = projectile->GetParentCell()) {
            if (auto* bhkWorld = cell->GetbhkWorld()) {
                if (auto* world = bhkWorld->GetWorld1()) {
                    worldGravityZ = world->gravity.quad.m128_f32[2];
                }
            }
        }

        auto* projectileBase = projectile->GetProjectileBase();
        if (!projectileBase) {
            return false;
        }

        auto& projectileData = projectile->GetProjectileRuntimeData();
        auto* gameSettings = RE::GameSettingCollection::GetSingleton();
        auto* weakGravitySetting = gameSettings ? gameSettings->GetSetting("fArrowWeakGravity") : nullptr;
        if (!weakGravitySetting) {
            log::warn("[arrowInterpreter] Could not read fArrowWeakGravity");
            return false;
        }

        // For a same-height parabola with a requested apex and duration: apexHeight = gravity * flightTime^2 / 8
        // GetGravity() returns a multiplier applied to Havok gravity and the 59 game-unit conversion, so solve for that multiplier first.
        const float requiredGravity = 8.0f * apexHeight / (flightTime * flightTime);
        const float gravityUnit = std::abs(worldGravityZ) * havokToGameUnits;
        if (gravityUnit <= 0.0f) {
            return false;
        }

        const float requiredGravityMultiplier = requiredGravity / gravityUnit;
        const float weakGravity = weakGravitySetting->GetFloat();
        const float recordGravity = projectileBase->data.gravity;
        const float gravityRange = weakGravity - recordGravity;
        if (std::abs(gravityRange) <= 0.000001f) {
            return false;
        }

        // ArrowProjectile::GetGravity(): weakGravity - ((weakGravity - recordGravity) * power) from ghidra, so use neg values to tune grav
        projectileData.power = (weakGravity - requiredGravityMultiplier) / gravityRange;

        const float actualGravity =
            std::abs(worldGravityZ) * projectile->GetGravity() * havokToGameUnits;
        log::info(
            "[arrowInterpreter] arrowRain gravity requested={}, actual={}, multiplier={}, power={}",
            requiredGravity,
            actualGravity,
            requiredGravityMultiplier,
            projectileData.power);
        
        auto& velocity = projectileData.linearVelocity;
        const float oldHorizontalSpeed = std::hypot(velocity.x, velocity.y);
        if (oldHorizontalSpeed <= 0.001f) {
            return false;
        }
        const float forwardX = velocity.x / oldHorizontalSpeed;
        const float forwardY = velocity.y / oldHorizontalSpeed;
        const float rightX = forwardY;
        const float rightY = -forwardX;

        const float displacementX = forwardX * targetForward + rightX * targetLateral;
        const float displacementY = forwardY * targetForward + rightY * targetLateral;

        velocity.x = displacementX / flightTime;
        velocity.y = displacementY / flightTime;
        velocity.z = 4.0f * apexHeight / flightTime;
        log::info("[arrowInterpreter] Arrow rain velocity=({}, {}, {})", velocity.x, velocity.y, velocity.z);
        return true;
    }

    void ProcessEventHook::InitProjectile(RE::Projectile* a_this) { 
        _InitProjectile(a_this);
        //log::info("[arrowInterpreter] Init Hook ran");
        //log::info("[arrowInterpreter] Init ptr={}, pending={}", static_cast<void*>(a_this), it != pendingArrows.end());
        float offset = 0.0f;
        ArrowData pending{};
        {
            std::scoped_lock lock(pendingArrowsMutex);

            auto it = pendingArrows.find(a_this);
            if (it == pendingArrows.end()) {
                return;
            }

            pending = it->second;
            pendingArrows.erase(it);
        }
        offset = pending.offset;
        auto& velocity = a_this->GetProjectileRuntimeData().linearVelocity;
        const float x = velocity.x;
        const float y = velocity.y;
        if (pending.isArrowRain) {
            log::info("[arrowInterpreter] arrowRain Before velocity=({}, {}, {})", velocity.x, velocity.y, velocity.z);
            calculateNewVelocity(
                a_this,
                pending.targetForward,
                pending.targetLateral,
                pending.apex,
                pending.duration);
        
        } else {
            const float c = std::cos(offset);
            const float s = std::sin(offset);
            velocity.x = x * c - y * s;
            velocity.y = x * s + y * c;
            //log::info("[arrowInterpreter] Modified velocity=({}, {}, {})", velocity.x, velocity.y, velocity.z);
            
            // modify proj rotation visually
            auto point = a_this->GetAngle();
            //log::info("[arrowInterpreter] Original Angle=({}, {}, {})", point.x, point.y, point.z);
            point.z -= offset;
            a_this->SetAngle(point);
            //log::info("[arrowInterpreter] Original Angle=({}, {}, {})", 
            //    a_this->GetAngle().x, a_this->GetAngle().y,a_this->GetAngle().z);
        }
        
    }
}
