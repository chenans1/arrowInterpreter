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

        //log::info(
        //    "[arrowInterpreter] Launch transform: "
        //    "origin=({}, {}, {}), pitch={}, yaw={}",
        //    origin.x, origin.y, origin.z, rotation.x, rotation.z);

        RE::ProjectileHandle handle;
        RE::Projectile::LaunchArrow(&handle, actor, ammo, weapon, origin, rotation);

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

        log::info(
            "[arrowInterpreter] After correction: "
            "power={}, weaponDamage={}",
            projectileData.power, projectileData.weaponDamage);
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