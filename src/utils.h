#pragma once

namespace utils {
    static inline std::vector<RE::ActorHandle> FindActorsNearImpact(RE::Projectile* projectile, const RE::NiPoint3& impactPoint,float radius) {
        std::vector<RE::ActorHandle> actors;
        auto* cell = projectile ? projectile->GetParentCell() : nullptr;
        if (!cell || !cell->IsAttached() || radius <= 0.0f) {
            return actors;
        }

        radius = std::min(radius, 4095.0f);
        cell->ForEachReferenceInRange(impactPoint, radius, [&](RE::TESObjectREFR* ref){
            auto* actor = ref ? ref->As<RE::Actor>() : nullptr;
            if (!actor ||
                // actor->IsDead() ||
                actor->IsDisabled() ||
                !actor->Is3DLoaded()) {
                return RE::BSContainer::ForEachResult::kContinue;
            }
            // SKSE::log::info( "[Utils] added={:08X} to actorsVec", actor ? actor->GetFormID() : 0);
            actors.emplace_back(actor->GetHandle());
            return RE::BSContainer::ForEachResult::kContinue;
        });
        return actors;
    }

    //compute forward offset player. gonna do some hacky trig
    static inline std::optional<float> calculateCrosshairForwardOffset(const RE::Actor* actor, const RE::NiPoint3& launchOrigin) {
        if (!actor || actor != RE::PlayerCharacter::GetSingleton()) {
            return std::nullopt;
        }

        const auto* pickData = RE::CrosshairPickData::GetSingleton();
        if (!pickData) {
            return std::nullopt;
        }

        const auto& target = pickData->collisionPoint;
        if (!std::isfinite(target.x) ||
            !std::isfinite(target.y) ||
            !std::isfinite(target.z)) {
            return std::nullopt;
        }

        const float deltaX = target.x - launchOrigin.x;
        const float deltaY = target.y - launchOrigin.y;
        const float horizontalDistance = std::hypot(deltaX, deltaY);

        constexpr float minimumDistance = 32.0f;
        constexpr float maximumDistance = 10000.0f;
        if (horizontalDistance < minimumDistance || horizontalDistance > maximumDistance) {
            SKSE::log::info(
                "[arrowInterpreter] Crosshair target rejected: point=({}, {}, {}), horizontalDistance={}"
                ,target.x,target.y,target.z,horizontalDistance);
            return std::nullopt;
        }

        const float heading = actor->GetAimHeading();
        const float forwardX = std::sin(heading);
        const float forwardY = std::cos(heading);
        const float rightX = forwardY;
        const float rightY = -forwardX;
        const float forwardOffset = deltaX * forwardX + deltaY * forwardY;
        const float lateralOffset = deltaX * rightX + deltaY * rightY;

        if (!std::isfinite(forwardOffset) || forwardOffset < minimumDistance) {
            SKSE::log::info(
                "[arrowInterpreter] Crosshair target rejected: forwardOffset={}, horizontalDistance={}",
                forwardOffset,horizontalDistance);
            return std::nullopt;
        }

        SKSE::log::info("[arrowInterpreter] Crosshair target: origin=({}, {}, {}), point=({}, {}, {}), forwardOffset={}, lateralError={}",
            launchOrigin.x, launchOrigin.y, launchOrigin.z,
            target.x, target.y, target.z,
            forwardOffset, lateralOffset);
        
        // return sqrt(forwardOffset*forwardOffset + lateralOffset*lateralOffset);
        return forwardOffset;
    }

    //compute forward offset npc gonan just be checking combat target and calc distance
    
}