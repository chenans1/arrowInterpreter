#pragma once
#include "utils.h"
#include "hitDataAPI.h"
#include "processEventHook.h"

class arrowHook{
    public:
        static void Install() {
            SKSE::log::info("[ArrowImpactHook] attemping to install ArrowProjectile::AddImpact");
            REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE_ArrowProjectile[0]};
            _original = vtable.write_vfunc(0xBD, AddImpact);
            SKSE::log::info("[ArrowImpactHook] Installed");
        }

    private:
        //apparently clib is capping, on both 1170 and 97 addimpact doesnt actually return void, it returns impact data
        static RE::Projectile::ImpactData* AddImpact(RE::ArrowProjectile* a_projectile, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc, const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6, std::uint32_t a_arg7) {
            // _original(a_projectile, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
            
            //check if we are one of the arrows in arrow rain, if not ignore. 
            float search_radius = 0.0f;
            bool isArrowRain = false;
            {
                std::scoped_lock lock(arrow::ProcessEventHook::ARmutex);

                auto it = arrow::ProcessEventHook::ARarrows.find(a_projectile);
                if (it != arrow::ProcessEventHook::ARarrows.end()) {
                    search_radius = it->second;
                    arrow::ProcessEventHook::ARarrows.erase(it);
                    isArrowRain = true;
                }
            }
            if (!isArrowRain){ 
                // _original(a_projectile, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7); return;
                return _original(a_projectile, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
            }
            //reset the power data for the projectile here, seems to cause issues with damage calc?
            auto& projectileData = a_projectile->GetProjectileRuntimeData();
            projectileData.power = 1.0f;
            const RE::NiPoint3 impactPoint = a_targetLoc;
            // SKSE::log::info("[ArrowImpactHook] impact ref={:08X}, position=({}, {}, {})",
            //     a_ref ? a_ref->GetFormID() : 0, impactPoint.x, impactPoint.y, impactPoint.z);
            const std::vector<RE::ActorHandle> actorsVec = utils::FindActorsNearImpact(a_projectile, impactPoint, search_radius);
            const auto* directlyHitActor = a_ref ? a_ref->As<RE::Actor>() : nullptr;
            const auto shooter = a_projectile ->GetProjectileRuntimeData().shooter.get();
            if (!shooter) {
                SKSE::log::info("[ArrowImpactHook] AddImpact: no shooter");
                // _original(a_projectile, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
                // return;
                return _original(a_projectile, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
            }

            auto* impact = _original(a_projectile, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
            
            for (const auto& actorHandle : actorsVec) {
                auto actor = actorHandle.get();
                if (!actor) {
                    // SKSE::log::warn("[ArrowImpactHook] AddImpact: invalid actor");
                    continue;
                }
                if (actor.get() == directlyHitActor) {
                    // SKSE::log::info("[ArrowImpactHook] AddImpact: Skip direct target {:08X}", actor->GetFormID());
                    continue;
                }
                if (shooter && actor.get() == shooter.get()) {
                    // SKSE::log::info("[ArrowImpactHook] AddImpact: Skipping shooter {:08X}", actor->GetFormID());
                    continue;
                }
                if (!arrow::ApplyArrowHit(a_projectile, actor.get())) {
                    // SKSE::log::warn("[ArrowImpactHook] Failed radial hit for {:08X}", actor->GetFormID());
                    continue;
                } 
                // else {
                //     SKSE::log::info("[ArrowImpactHook] AddImpact: projectile={:p} Sucessfully found target", static_cast<void*>(a_projectile));
                // }
            }
            a_projectile->GetMissileRuntimeData().impactResult = RE::ImpactResult::kDestroy;
            // return _original(a_projectile, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
            if (impact) {
                impact->impactResult = RE::ImpactResult::kDestroy;
            }
            return impact;
        }   

        static inline REL::Relocation<decltype(AddImpact)> _original;
};