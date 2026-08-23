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

    //running this install separated - this way I can probably chain hook TDM and do it right after
    void ProcessEventHook::InstallProjectileHook() {
        log::info("[processEventHook] Installing Projectile Hook");
        auto& trampoline = SKSE::GetTrampoline();

        //hook locations derived from TDM, doing a hook after TDM to avoid dll alphabetical sorting shenanigans
        REL::Relocation<std::uintptr_t> hook{RELOCATION_ID(43030, 44222)};

        _InitProjectile = trampoline.write_call<5>(hook.address() + REL::Relocate(0x3B8, 0x78A), InitProjectile);
        log::info("[processEventHook] ...Projectile hook installed");
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

        const auto& biped = actor->GetBiped2();
        RE::NiAVObject* weapon3D = nullptr;

        //fetch the bow location, not sure if this is a good idea depends on whether the anim features draw pull or not? IDK.
        for (std::size_t i = 0; i < RE::BIPED_OBJECTS::kTotal; ++i) {
            auto& object = biped->objects[i];

            if (object.item == weapon && object.partClone) {
                weapon3D = object.partClone.get();
                //log::info("[arrowInterpreter] Weapon biped slot {}: partClone={}, name={}", i, static_cast<void*>(weapon3D), weapon3D->name.c_str());
                break;
            }
        }
        RE::NiPoint3 origin;
        RE::Projectile::ProjectileRot rotation{};

        if (weapon3D) {
            origin = weapon3D->world.translate;
            rotation.x = actor->GetAimAngle();
            rotation.z = actor->GetAimHeading();
        } else {
            origin = actor->GetPosition();
            origin.z += 96.0f;
            rotation.x = actor->GetAimAngle();
            rotation.z = actor->GetAimHeading();
            log::warn("[arrowInterpreter] No weapon/fire node; using actor fallback");
        }

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
            rotation.x = actor->GetAimAngle();
            rotation.z = actor->GetAimHeading();
        } else {
            origin = actor->GetPosition();
            origin.z += 96.0f;
            rotation.x = actor->GetAimAngle();
            rotation.z = actor->GetAimHeading();
            log::warn("[arrowInterpreter] No weapon/fire node; using actor fallback");
        }
        //rotation.z += 1.5708f;
        //origin.y += 96.0f;
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
        auto& projectileData = projectile->GetProjectileRuntimeData();
        if (projectileData.power > 0.0f) {
            projectileData.weaponDamage /= projectileData.power;
            projectileData.power = 1.0f;
            projectileData.weaponDamage *= damageMult;
        }

        //add the arrow to the pendingArrows map so that in getlinearvelocity we update the value
        ProcessEventHook::AddPendingArrow(projectile.get(), 1.5708f);
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

    void ProcessEventHook::AddPendingArrow(const RE::Projectile* a_projectile, float a_offset) {
        //std::scoped_lock lock(pendingArrowsMutex);
        pendingArrows.emplace(a_projectile, a_offset);
        log::info("[arrowInterpreter] Stored Arrow");
    }

    void ProcessEventHook::InitProjectile(RE::Projectile* a_this) { 
        _InitProjectile(a_this);
        //log::info("[arrowInterpreter] Init Hook ran");
        auto it = pendingArrows.find(a_this);

        //log::info("[arrowInterpreter] Init ptr={}, pending={}", static_cast<void*>(a_this), it != pendingArrows.end());
        float offset = 0.0f;
        if (it == pendingArrows.end()) {
            return;
        }
        offset = it->second;
        pendingArrows.erase(it);
        auto& velocity = a_this->GetProjectileRuntimeData().linearVelocity;

        const float c = std::cos(offset);
        const float s = std::sin(offset);

        const float x = velocity.x;
        const float y = velocity.y;
        log::info("[arrowInterpreter] Before velocity=({}, {}, {})", velocity.x, velocity.y, velocity.z);

        velocity.x = x * c - y * s;
        velocity.y = x * s + y * c;
        log::info("[arrowInterpreter] Modified velocity=({}, {}, {})", velocity.x, velocity.y, velocity.z);

    }

}