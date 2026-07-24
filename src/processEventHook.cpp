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

    static bool releaseArrow(RE::TESAmmo* ammo, RE::TESObjectWEAP* weapon, RE::Actor* actor) {
        if (!ammo || !weapon || !actor) {
            log::info("[arrowInterpreter] invalid params for releaseArrow()");
            return false;
        }

        auto* currentProcess = actor->GetActorRuntimeData().currentProcess;
        if (!currentProcess) {
            log::warn("[arrowInterpreter] Actor {:08X} has no current process", actor->GetFormID());
            return false;
        }

        auto* weaponNode = currentProcess->GetWeaponNode(actor->GetBiped2());
        if (!weaponNode) {
            log::warn("[arrowInterpreter] Actor {:08X} has no weapon node", actor->GetFormID());
            return false;
        }

        RE::ProjectileHandle handle;
        RE::Projectile::LaunchArrow(&handle, actor, ammo, weapon);
        auto projectile = handle.get();
        if (!projectile) {
            log::error("[arrowInterpreter] Failed to launch arrow for actor {:08X}", actor->GetFormID());
            return false;
        }
        auto& projectileData = projectile->GetProjectileRuntimeData();
        if (projectileData.power > 0.0f) {
            projectileData.weaponDamage /= projectileData.power;
            projectileData.power = 1.0f;
            projectileData.weaponDamage *= projectileData.power;
        }

        log::info(
            "[arrowInterpreter] After correction: "
            "power={}, weaponDamage={}, speedMult={}",
            projectileData.power, projectileData.weaponDamage, projectileData.speedMult);
        return true;
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
        //launch an arrow
        releaseArrow(ammo, bow, actor);

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