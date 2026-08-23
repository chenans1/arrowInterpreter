#include "PCH.h"
#include "processEventHook.h"

using namespace SKSE;
using namespace SKSE::log;
using namespace std::literals;

namespace arrow {
    //doing npcs and pcs
    void ProcessEventHook::Install() { 
        log::info("[processEventHook] Install ProcessEvent() Hook");

        REL::Relocation<std::uintptr_t> vtblNPC{RE::VTABLE_Character[2]};
        REL::Relocation<std::uintptr_t> vtblPC{RE::VTABLE_PlayerCharacter[2]};

        _originalNPC = vtblNPC.write_vfunc(0x1, ProcessEvent_NPC);
        _originalPC = vtblPC.write_vfunc(0x1, ProcessEvent_PC);

        log::info("[processEventHook] ...ProcessEvent hook installed");
    }

    static bool releaseArrow(RE::TESAmmo* ammo, RE::TESObjectWEAP* weapon, RE::Actor* actor, float damageMult = 1.0f) {
        if (!ammo || !weapon || !actor) {
            log::info("[arrowInterpreter] invalid params for releaseArrow()");
            return false;
        }

        auto* currentProcess = actor->GetActorRuntimeData().currentProcess;
        if (!currentProcess) {
            log::warn("[arrowInterpreter] Actor {:08X} has no current process", actor->GetFormID());
            return false;
        }

        //RE::NiPoint3 origin = fireNode->world.translate;
        RE::NiPoint3 origin = actor->GetPosition();
        origin.z += 96.0f;
        //RE::NiPoint3 origin = weaponNode->world.translate;
        RE::Projectile::ProjectileRot rotation{};

        rotation.x = actor->GetAimAngle();
        rotation.z = actor->GetAimHeading();
        log::info(
            "[arrowInterpreter] Launch transform: origin=({}, {}, {}), pitch={}, yaw={}",
            origin.x, origin.y, origin.z, rotation.x, rotation.z);

        RE::ProjectileHandle handle;
        //RE::Projectile::LaunchArrow(&handle, actor, ammo, weapon, origin, rotation);
        RE::Projectile::LaunchData launchData(actor, origin, rotation, ammo, weapon);

        launchData.autoAim = false;
        launchData.desiredTarget = nullptr;
        RE::Projectile::Launch(&handle, launchData);

        auto projectile = handle.get();
        //RE::NiPoint3 point = projectile->GetAngle();
        //Quaternion main = Quaternion::CreateFromYawPitchRoll();
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
        return true;
    }

    static bool releaseArrowOffset(RE::TESAmmo* ammo, RE::TESObjectWEAP* weapon, RE::Actor* actor, float damageMult = 1.0f) {
        if (!ammo || !weapon || !actor) {
            log::info("[arrowInterpreter] invalid params for releaseArrow()");
            return false;
        }

        auto* currentProcess = actor->GetActorRuntimeData().currentProcess;
        if (!currentProcess) {
            log::warn("[arrowInterpreter] Actor {:08X} has no current process", actor->GetFormID());
            return false;
        }

        // RE::NiPoint3 origin = fireNode->world.translate;
        RE::NiPoint3 origin = actor->GetPosition();

        RE::NiPoint3 origin = 
        origin.z += 96.0f;
        // RE::NiPoint3 origin = weaponNode->world.translate;
        //gonna offset the Y temporarily for visual clarity purposes
        origin.y += 96.0f;
        //origin.x += 96.0f;

        RE::Projectile::ProjectileRot rotation{};
        rotation.x = actor->GetAimAngle();
        rotation.z = actor->GetAimHeading();

        //rotation.z += 1.0472f; //60 def offset clockwise

        RE::ProjectileHandle handle;
        RE::Projectile::LaunchData launchData(actor, origin, rotation, ammo, weapon);
        launchData.autoAim = false;
        launchData.desiredTarget = nullptr;
        RE::Projectile::Launch(&handle, launchData);
        auto projectile = handle.get();
        if (!projectile) {
            log::error("[arrowInterpreter] Failed to launch arrow for actor {:08X}", actor->GetFormID());
            return false;
        }
        //from noahboddie: quarternion rotation multiplication
        auto point = projectile->GetAngle();
        Quaternion current = Quaternion::CreateFromYawPitchRoll(Vector3{point.x, point.y, point.z});
        log::info("Before SetAngle: ({}, {}, {})", point.x, point.y, point.z);
        Quaternion offset = Quaternion::CreateFromYawPitchRoll(Vector3{0.0f, 0.0f, 1.0472f});
        Quaternion combined = offset * current;
        Vector3 result = combined.ToEuler();
        projectile->SetAngle(RE::NiPoint3{result.x, result.y, result.z});
        auto& projectileData = projectile->GetProjectileRuntimeData();
        if (projectileData.power > 0.0f) {
            projectileData.weaponDamage /= projectileData.power;
            projectileData.power = 1.0f;
            projectileData.weaponDamage *= damageMult;
        }

        log::info("After SetAngle: ({}, {}, {})", result.x, result.y, result.z);

        //from noahboddie: quarternion rotation multiplication
        //auto point = projectile->GetAngle();
        //Quaternion current = Quaternion::CreateFromYawPitchRoll(Vector3{point.x, point.y, point.z});
        //log::info("Before SetAngle: ({}, {}, {})", point.x, point.y, point.z);
        //Quaternion offset = Quaternion::CreateFromYawPitchRoll(Vector3{0.0f, 0.0f, 1.0472f});
        //Quaternion combined = offset * current;
        //Vector3 result = combined.ToEuler();

        //projectile->SetAngle(RE::NiPoint3{result.x, result.y, result.z});

        //log::info("After SetAngle: ({}, {}, {})", result.x, result.y, result.z);
        
        //auto point = projectile->GetAngle();

        //log::info("Before SetAngle: ({}, {}, {})", point.x, point.y, point.z);

        //point.z += RE::deg_to_rad(60.0f);

        //projectile->SetAngle(point);

        //log::info("After SetAngle: ({}, {}, {})", point.x, point.y, point.z);

        //log::info("[arrowInterpreter] Offset Arrow Transforms: origin=({}, {}, {}), pitch={}, yaw={}",
        //     origin.x, origin.y, origin.z, rotation.x, rotation.z);
        //log::info("[arrowInterpreter] Offset Launch velocities: velocity=({}, {}, {}), linearVelocity=({}, {}, {}),",
        //          projectileData.velocity.x, projectileData.velocity.y, projectileData.velocity.z,
        //          projectileData.linearVelocity.x, projectileData.linearVelocity.y, projectileData.linearVelocity.z);

        return true;
    }

    //process stringview into float basic implementation for testing
    static float parse(std::string_view payload, float defaultValue = 1.0f) {
        constexpr std::string_view prefix = "dmg=";
        if (!payload.starts_with(prefix)) {
            return defaultValue;
        }
        payload.remove_prefix(prefix.size());

        if (payload.empty()) {
            return defaultValue;
        }

        float value = defaultValue;

        const char* begin = payload.data();
        const char* end = begin + payload.size();

        const auto [ptr, error] = std::from_chars(begin, end, value);
        if (error != std::errc{} || ptr != end) {
            return defaultValue;
        }

        return value;
    }

    //checks if it's out tag
    static void HandleEvent(RE::BSAnimationGraphEvent* a_event) {
        if (!a_event || !a_event->holder || !a_event->tag.data()) return;
        auto* holder = const_cast<RE::TESObjectREFR*>(a_event->holder);
        if (!holder) return;
        auto* actor = holder ? holder->As<RE::Actor>() : nullptr;
        if (!actor) return;

        const auto& tag = a_event->tag;
        const auto& payload = a_event->payload;
        if (tag != "arrowInterpreter"sv) { return; }
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

        //process payload now 
        //going with format is like dmg=float|count=int{1, 15}|spread=float{0, 360}
        //eg: arrowinterpreter.dmg=0.5|count=3|spread=45.0
        //launch an arrow
        float mult = parse(payload);
        if (releaseArrow(ammo, bow, actor, mult)) {
            actor->UseAmmo(1);
            releaseArrowOffset(ammo, bow, actor, mult);
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

}