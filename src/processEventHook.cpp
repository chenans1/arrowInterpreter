#include "PCH.h"
#include "processEventHook.h"
#include <unordered_set>

using namespace SKSE;
using namespace SKSE::log;
using namespace std::literals;

//REL::Relocation<uintptr_t> AnimEventVtbl_NPC{RE::VTABLE_Character[2]};
//_ProcessEvent = AnimEventVtbl_NPC.write_vfunc(0x1, ProcessEvent);

namespace draugr {

    enum class DraugrWeaponClass { kUnknown, kOneHanded, kGreatsword, kTwoHanded };

    //checked from the draugr Race Record
    static const std::unordered_set<std::string_view> kNormalAttacks{
        "attackStart1HMSwipe",
        "attackStart1HMBackSlash",
        "attackStart1HMX2",
        "attackStartGSBackSlash", 
        "attackStartGSChop",          
        "attackStartGSX2",
        "attackStart2HMSlash", 
        "attackStart2HMForwardSwipe", 
        "attackStart2HMBackSwipe",

        // Hand to Hand (to be implemented)
        // "attackStartH2HLeft",
        // "attackStartH2HRight",
    };

    static const std::unordered_set<std::string_view> kPowerAttacks{
        "attackStart1HMPowerChop",       
        "attackStart1HMForwardPower", 
        "attackStart1HMPowerSlash",

        "attackStartGSForwardPowerB",

        "attackStart2HMForwardPowerChop", 
        "attackStart2HMPowerChop",
    };

    // output targets

    struct AttackTargets {
        std::string_view normal;
        std::string_view power;
    };
    //final rerouted animevents
    static constexpr AttackTargets k1HM{"attackStart1HMSwipe", "attackStart1HMPowerSlash"};
    static constexpr AttackTargets k2HM{"attackStartGSChop", "attackStartGSForwardPowerB"};
    static constexpr AttackTargets k2HW{"attackStart2HMSlash","attackStart2HMForwardPowerChop"};

    static DraugrWeaponClass GetWeaponClass(RE::Actor* actor) {
        if (!actor) {
            return DraugrWeaponClass::kUnknown;
        }

        // false = right hand
        auto* equipped = actor->GetEquippedObject(false);

        if (!equipped) {
            return DraugrWeaponClass::kUnknown;
        }

        auto* weapon = equipped->As<RE::TESObjectWEAP>();

        if (!weapon) {
            return DraugrWeaponClass::kUnknown;
        }

        switch (weapon->GetWeaponType()) {
            case RE::WEAPON_TYPE::kOneHandSword:
            case RE::WEAPON_TYPE::kOneHandDagger:
            case RE::WEAPON_TYPE::kOneHandAxe:
            case RE::WEAPON_TYPE::kOneHandMace:
                return DraugrWeaponClass::kOneHanded;

            case RE::WEAPON_TYPE::kTwoHandSword:
                return DraugrWeaponClass::kGreatsword;

            case RE::WEAPON_TYPE::kTwoHandAxe:
                // Skyrim uses this category for the 2H axe/hammer family.
                return DraugrWeaponClass::kTwoHanded;

            default:
                return DraugrWeaponClass::kUnknown;
        }
    }

    static std::string_view GetReroutedEvent(DraugrWeaponClass weaponClass, bool powerAttack) {
        const AttackTargets* targets = nullptr;

        switch (weaponClass) {
            case DraugrWeaponClass::kOneHanded:
                targets = &k1HM;
                break;

            case DraugrWeaponClass::kGreatsword:
                targets = &k2HM;
                break;

            case DraugrWeaponClass::kTwoHanded:
                targets = &k2HW;
                break;

            default:
                return {};
        }

        return powerAttack ? targets->power : targets->normal;
    }

    bool ProcessEventHook::NotifyAnimationGraph_NPC(
        RE::IAnimationGraphManagerHolder* a_this,const RE::BSFixedString& a_eventName) {
        if (!a_this) {
            log::info("[DraugrAttackReroute] no a_this");
            return _originalNPC(a_this, a_eventName);
        }
        const std::string_view tag{a_eventName.c_str()};

        bool isPowerAttack = false;
        if (kNormalAttacks.contains(tag)) {
            isPowerAttack = false;
        } else if (kPowerAttacks.contains(tag)) {
            isPowerAttack = true;
        } else {
            return _originalNPC(a_this, a_eventName);
        }

        auto* refr = SKSE::stl::adjust_pointer<RE::TESObjectREFR>(a_this, -0x38);

        if (!refr) {
            log::info("[DraugrAttackReroute] ref pointer substraction failed");
            return _originalNPC(a_this, a_eventName);
        }

        auto* actor = refr->As<RE::Actor>();

        if (!actor) {
            log::info("[DraugrAttackReroute] no actor");
            return _originalNPC(a_this, a_eventName);
        }

        const auto weaponClass = GetWeaponClass(actor);

        if (weaponClass == DraugrWeaponClass::kUnknown) {
            return _originalNPC(a_this, a_eventName);
        }
        const auto reroutedEvent = GetReroutedEvent(weaponClass, isPowerAttack);

        if (reroutedEvent.empty()) {
            log::info("[DraugrAttackReroute] N/A AnimEvent={}", tag);
            return _originalNPC(a_this, a_eventName);
        }
        log::info("[DraugrAttackReroute] Actor={} Event={} -> {}", actor->GetName(), tag, reroutedEvent);
        const RE::BSFixedString replacement{reroutedEvent.data()};
        return _originalNPC(a_this, replacement);
    }

    void ProcessEventHook::Install() {
        log::info("[NotifyAnimationGraphHook] Installing Character hook");

        REL::Relocation<std::uintptr_t> vtblNPC{RE::VTABLE_Character[3]};

        _originalNPC = vtblNPC.write_vfunc(0x1, NotifyAnimationGraph_NPC);

        log::info("[NotifyAnimationGraphHook] Character hook installed");
    }
}