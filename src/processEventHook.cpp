#include "PCH.h"
#include "processEventHook.h"
#include <unordered_set>

using namespace SKSE;
using namespace SKSE::log;
using namespace std::literals;

namespace draugr {
    //checks to see if the race has draugr behaviors
    static bool ContainsDraugr(std::string_view str) {
        constexpr std::string_view target = "draugr";

        if (str.size() < target.size()) {
            return false;
        }

        for (std::size_t i = 0; i <= str.size() - target.size(); ++i) {
            bool match = true;

            for (std::size_t j = 0; j < target.size(); ++j) {
                const auto c = static_cast<unsigned char>(str[i + j]);

                if (std::tolower(c) != target[j]) {
                    match = false;
                    break;
                }
            }

            if (match) {
                return true;
            }
        }

        return false;
    }

    static bool IsDraugrBehavior(RE::Actor* actor) {
        if (!actor) {
            return false;
        }

        auto* race = actor->GetRace();
        auto* actorBase = actor->GetActorBase();

        if (!race || !actorBase) {
            return false;
        }

        const auto sex = actorBase->GetSex();

        const char* projectName = race->behaviorGraphProjectNames[sex].c_str();

        if (!projectName || !*projectName) {
            return false;
        }

        return ContainsDraugr(projectName);
    }

    enum class DraugrWeaponClass { kUnknown, kUnarmed, kOneHanded, kGreatsword, kTwoHanded };

    //checked from the draugr Race Record. attackstart and attackpowerstartinplace were added to handle SCAR 2.0 attackdata functionality
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
         "attackStartH2HLeft",
        "attackStartH2HRight",

        "SCAR_DraugrNA", 
        "attackStart",
    };

    static const std::unordered_set<std::string_view> kPowerAttacks{
        "attackStart1HMPowerChop",       
        "attackStart1HMForwardPower", 
        "attackStart1HMPowerSlash",
        
        "attackStartGSForwardPowerB",

        "attackStart2HMForwardPowerChop", 
        "attackStart2HMPowerChop",

        "SCAR_DraugrPA", 
        "attackPowerStartInPlace",
    };

    struct AttackTargets {
        RE::BSFixedString normal;
        RE::BSFixedString power;
    };

    static const AttackTargets k1HM{"attackStart1HMSwipe", "attackStart1HMPowerSlash"};

    static const AttackTargets k2HM{"attackStartGSChop", "attackStartGSForwardPowerB"};

    static const AttackTargets k2HW{"attackStart2HMSlash", "attackStart2HMPowerChop"};

    static const AttackTargets k0HM{"attackStartH2HLeft", "attackStartH2HLeft"};

    static DraugrWeaponClass GetWeaponClass(RE::Actor* actor) {
        if (!actor) {
            return DraugrWeaponClass::kUnknown;
        }

        // false = right hand
        auto* equipped = actor->GetEquippedObject(false);

        if (!equipped) {
            //vanilla unarmed draugrs pass through here, probably just assume unarmed in this case.
            //log::info("[DraugrAttackReroute] NoEquippedObject");
            //return DraugrWeaponClass::kUnknown;
            return DraugrWeaponClass::kUnarmed;
        }

        auto* weapon = equipped->As<RE::TESObjectWEAP>();

        if (!weapon) {
            //log::info("[DraugrAttackReroute] No Weapon");
            return DraugrWeaponClass::kUnknown;
        }

        switch (weapon->GetWeaponType()) {
            case RE::WEAPON_TYPE::kHandToHandMelee:
                //log::info("[DraugrAttackReroute] kHandToHandMelee weapontype");
                return DraugrWeaponClass::kUnarmed;

            case RE::WEAPON_TYPE::kOneHandSword:
            case RE::WEAPON_TYPE::kOneHandDagger:
            case RE::WEAPON_TYPE::kOneHandAxe:
            case RE::WEAPON_TYPE::kOneHandMace:
                //log::info("[DraugrAttackReroute] k1HM weapontype");
                return DraugrWeaponClass::kOneHanded;

            case RE::WEAPON_TYPE::kTwoHandSword:
                //log::info("[DraugrAttackReroute] k2HM weapontype");
                return DraugrWeaponClass::kGreatsword;

            case RE::WEAPON_TYPE::kTwoHandAxe:
                //log::info("[DraugrAttackReroute] k2HW weapontype");
                return DraugrWeaponClass::kTwoHanded;

            default:
                //log::info("[DraugrAttackReroute] kUnknown");
                return DraugrWeaponClass::kUnknown;
        }
    }

    static const RE::BSFixedString* GetReroutedEvent(DraugrWeaponClass a_weaponClass, bool a_powerAttack) {
        switch (a_weaponClass) {
            case DraugrWeaponClass::kUnarmed:
                //log::info("[DraugrAttackReroute] no rerouted UnarmedEvent");
                return a_powerAttack ? &k0HM.power : &k0HM.normal;

            case DraugrWeaponClass::kOneHanded:
                return a_powerAttack ? &k1HM.power : &k1HM.normal;

            case DraugrWeaponClass::kGreatsword:
                return a_powerAttack ? &k2HM.power : &k2HM.normal;

            case DraugrWeaponClass::kTwoHanded:
                return a_powerAttack ? &k2HW.power : &k2HW.normal;

            default:
                return nullptr;
        }
    }
    bool ProcessEventHook::NotifyAnimationGraph_NPC(
        RE::IAnimationGraphManagerHolder* a_this,const RE::BSFixedString& a_eventName) {
        if (!a_this) {
            //log::info("[DraugrAttackReroute] no a_this");
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
            return _originalNPC(a_this, a_eventName);
        }

        auto* actor = refr->As<RE::Actor>();

        if (!actor) {
            //log::info("[DraugrAttackReroute] no actor");
            return _originalNPC(a_this, a_eventName);
        }

        //needed due to attackStart/attackPowerStart being used across humanoid and draugrs
        if (!IsDraugrBehavior(actor)) {
            return _originalNPC(a_this, a_eventName);
        }

        const auto weaponClass = GetWeaponClass(actor);

        if (weaponClass == DraugrWeaponClass::kUnknown) {
            //log::info("[DraugrAttackReroute] unknown weapon class");
            return _originalNPC(a_this, a_eventName);
        }

        const auto* reroutedEvent = GetReroutedEvent(weaponClass, isPowerAttack);

        if (!reroutedEvent) {
            return _originalNPC(a_this, a_eventName);
        }

        if (a_eventName == *reroutedEvent) {
            return _originalNPC(a_this, a_eventName);
        }

        //log::info("[DraugrAttackReroute] Actor={} Event={} -> {}", actor->GetName(), a_eventName.c_str(), reroutedEvent->c_str());

        return _originalNPC(a_this, *reroutedEvent);
    }

    void ProcessEventHook::Install() {
        log::info("[NotifyAnimationGraphHook] Installing Character hook");

        REL::Relocation<std::uintptr_t> vtblNPC{RE::VTABLE_Character[3]};

        _originalNPC = vtblNPC.write_vfunc(0x1, NotifyAnimationGraph_NPC);

        log::info("[NotifyAnimationGraphHook] Character hook installed");
    }
}