#pragma once

namespace utils {
    std::vector<RE::ActorHandle> FindActorsNearImpact(RE::Projectile* projectile, const RE::NiPoint3& impactPoint,float radius) {
        std::vector<RE::ActorHandle> actors;
        auto* cell = projectile ? projectile->GetParentCell() : nullptr;
        if (!cell || !cell->IsAttached() || radius <= 0.0f) {
            return actors;
        }

        radius = std::min(radius, 4095.0f);
        cell->ForEachReferenceInRange(impactPoint, radius, [&](RE::TESObjectREFR* ref){
            auto* actor = ref ? ref->As<RE::Actor>() : nullptr;
            if (!actor ||
                actor->IsDead() ||
                actor->IsDisabled() ||
                !actor->Is3DLoaded()) {
                return RE::BSContainer::ForEachResult::kContinue;
            }
            SKSE::log::info( "[Utils] added={:08X} to actorsVec", actor ? actor->GetFormID() : 0);
            actors.emplace_back(actor->GetHandle());
            return RE::BSContainer::ForEachResult::kContinue;
        });
        return actors;
    }

}