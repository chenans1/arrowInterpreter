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

    // Uses the collision point maintained by Skyrim's normal crosshair picker.
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
            !std::isfinite(target.z) ||
            std::abs(target.x) > 1.0e20f ||
            std::abs(target.y) > 1.0e20f ||
            std::abs(target.z) > 1.0e20f) {
            SKSE::log::info("[arrowInterpreter] Native crosshair pick unavailable");
            return std::nullopt;
        }

        const float deltaX = target.x - launchOrigin.x;
        const float deltaY = target.y - launchOrigin.y;
        const float horizontalDistance = std::hypot(deltaX, deltaY);

        constexpr float minimumDistance = 32.0f;
        constexpr float maximumDistance = 10000.0f;
        if (horizontalDistance < minimumDistance || horizontalDistance > maximumDistance) {
            SKSE::log::info(
                "[arrowInterpreter] Native crosshair target rejected: point=({}, {}, {}), horizontalDistance={}"
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
            // SKSE::log::info("[arrowInterpreter] Native crosshair target rejected: forwardOffset={}, horizontalDistance={}",
            //     forwardOffset,horizontalDistance);
            return std::nullopt;
        }

        // SKSE::log::info("[arrowInterpreter] Native crosshair target: origin=({}, {}, {}), point=({}, {}, {}), forwardOffset={}, lateralError={}",
        //     launchOrigin.x, launchOrigin.y, launchOrigin.z,
        //     target.x, target.y, target.z,
        //     forwardOffset, lateralOffset);
        
        // return sqrt(forwardOffset*forwardOffset + lateralOffset*lateralOffset);
        return forwardOffset;
    }

    struct TargetOffsets {
        float forward;
        float lateral;
    };

    //Converts the centre of the rendered view into a world ray and asks the current Havok world for its first hit.
    static inline std::optional<TargetOffsets> calculateCrosshairRaycastForwardOffset(const RE::Actor* actor, const RE::NiPoint3& targetingOrigin, float maximumRayLength = 10000.0f) {
        if (!actor || actor != RE::PlayerCharacter::GetSingleton() || maximumRayLength <= 0.0f) {
            return std::nullopt;
        }

        auto* camera = RE::Main::WorldRootCamera();
        const auto screen = RE::BSGraphics::Renderer::GetScreenSize();
        if (!camera || screen.width == 0 || screen.height == 0) {
            SKSE::log::info("[arrowInterpreter] Crosshair raycast unavailable: no camera or screen size");
            return std::nullopt;
        }

        RE::NiPoint3 cameraOrigin{};
        RE::NiPoint3 direction{};
        if (!camera->WindowPointToRay(
                static_cast<std::int32_t>(screen.width / 2),
                static_cast<std::int32_t>(screen.height / 2),
                cameraOrigin,
                direction,
                static_cast<float>(screen.width),
                static_cast<float>(screen.height))) {
            SKSE::log::info("[arrowInterpreter] Crosshair raycast unavailable: WindowPointToRay failed");
            return std::nullopt;
        }

        const float directionLength = std::sqrt(
            direction.x * direction.x +
            direction.y * direction.y +
            direction.z * direction.z);

        if (!std::isfinite(directionLength) || directionLength <= 0.0001f) {
            return std::nullopt;
        }
        direction.x /= directionLength;
        direction.y /= directionLength;
        direction.z /= directionLength;

        // Start just beyond the player's depth along the exact camera ray. This preserves the crosshair line while avoiding an immediate hit on the player when the third-person camera is behind them.
        const float cameraToPlayer = std::sqrt(
            (targetingOrigin.x - cameraOrigin.x) * (targetingOrigin.x - cameraOrigin.x) +
            (targetingOrigin.y - cameraOrigin.y) * (targetingOrigin.y - cameraOrigin.y) +
            (targetingOrigin.z - cameraOrigin.z) * (targetingOrigin.z - cameraOrigin.z));
        const float startDistance = std::min(cameraToPlayer + 48.0f, maximumRayLength - 1.0f);
        if (!std::isfinite(startDistance) || startDistance < 0.0f) {
            return std::nullopt;
        }

        const RE::NiPoint3 rayStart{
            cameraOrigin.x + direction.x * startDistance,
            cameraOrigin.y + direction.y * startDistance,
            cameraOrigin.z + direction.z * startDistance
        };
        const RE::NiPoint3 rayEnd{
            cameraOrigin.x + direction.x * maximumRayLength,
            cameraOrigin.y + direction.y * maximumRayLength,
            cameraOrigin.z + direction.z * maximumRayLength
        };

        auto* cell = actor->GetParentCell();
        auto* physicsWorld = cell ? cell->GetbhkWorld() : nullptr;
        if (!physicsWorld) {
            SKSE::log::info("[arrowInterpreter] Crosshair raycast unavailable: no Havok world");
            return std::nullopt;
        }

        const float havokScale = RE::bhkWorld::GetWorldScale();
        if (!std::isfinite(havokScale) || havokScale <= 0.0f) {
            return std::nullopt;
        }

        RE::hkpClosestRayHitCollector collector{};
        collector.Reset();

        RE::bhkPickData pickData{};
        pickData.rayInput.from = RE::hkVector4{
            rayStart.x * havokScale,
            rayStart.y * havokScale,
            rayStart.z * havokScale,
            0.0f
        };
        pickData.rayInput.to = RE::hkVector4{};
        pickData.ray = RE::hkVector4{
            (rayEnd.x - rayStart.x) * havokScale,
            (rayEnd.y - rayStart.y) * havokScale,
            (rayEnd.z - rayStart.z) * havokScale,
            0.0f
        };
        pickData.rayHitCollectorA8 = &collector;

        physicsWorld->PickObject(pickData);
        if (!collector.HasHit()) {
            SKSE::log::info(
                "[arrowInterpreter] Crosshair raycast: no hit, camera=({}, {}, {}), direction=({}, {}, {})",
                cameraOrigin.x, cameraOrigin.y, cameraOrigin.z,
                direction.x, direction.y, direction.z);
            return std::nullopt;
        }

        const float hitFraction = collector.rayHit.hitFraction;
        const RE::NiPoint3 target{
            rayStart.x + (rayEnd.x - rayStart.x) * hitFraction,
            rayStart.y + (rayEnd.y - rayStart.y) * hitFraction,
            rayStart.z + (rayEnd.z - rayStart.z) * hitFraction
        };

        const float deltaX = target.x - targetingOrigin.x;
        const float deltaY = target.y - targetingOrigin.y;
        const float heading = actor->GetAimHeading();
        const float forwardX = std::sin(heading);
        const float forwardY = std::cos(heading);
        const float rightX = forwardY;
        const float rightY = -forwardX;
        const float forwardOffset = deltaX * forwardX + deltaY * forwardY;
        const float lateralOffset = deltaX * rightX + deltaY * rightY;

        // SKSE::log::info(
        //     "[arrowInterpreter] Crosshair raycast target: point=({}, {}, {}), forwardOffset={}, lateralError={}, hitFraction={}",
        //     target.x, target.y, target.z, forwardOffset, lateralOffset, hitFraction);

        if (!std::isfinite(forwardOffset) || forwardOffset < 128.0f) {
            return std::nullopt;
        }
        // return std::min(forwardOffset, maximumRayLength);
        return TargetOffsets{
            .forward = std::min(forwardOffset, maximumRayLength),
            .lateral = std::min(lateralOffset, 256.0f)
        };
    }

    // NPC prototype: assume the actor is aiming at its current combat target, making horizontal actor-to-target distance the desired forward offset.
    static inline std::optional<float> calculateCombatTargetForwardOffset(const RE::Actor* actor, const RE::NiPoint3& targetingOrigin, float maximumDistance = 10000.0f) {
        if (!actor || actor->IsPlayerRef() || maximumDistance <= 0.0f) {
            return std::nullopt;
        }

        const auto targetPtr = actor->GetActorRuntimeData().currentCombatTarget.get();
        if (!targetPtr || targetPtr.get() == actor || targetPtr->IsDisabled()) {
            SKSE::log::info("[arrowInterpreter] NPC {:08X} has no usable combat target", actor->GetFormID());
            return std::nullopt;
        }

        const auto targetPosition = targetPtr->GetPosition();
        const float deltaX = targetPosition.x - targetingOrigin.x;
        const float deltaY = targetPosition.y - targetingOrigin.y;
        const float horizontalDistance = std::hypot(deltaX, deltaY);

        constexpr float minimumDistance = 32.0f;
        if (!std::isfinite(horizontalDistance) ||
            horizontalDistance < minimumDistance ||
            horizontalDistance > maximumDistance) {
            // SKSE::log::info(
            //     "[arrowInterpreter] NPC combat target rejected: shooter={:08X}, target={:08X}, horizontalDistance={}",
            //     actor->GetFormID(), targetPtr->GetFormID(), horizontalDistance);
            return std::nullopt;
        }

        // SKSE::log::info(
        //     "[arrowInterpreter] NPC combat target: shooter={:08X}, target={:08X}, origin=({}, {}, {}), point=({}, {}, {}), forwardOffset={}",
        //     actor->GetFormID(), targetPtr->GetFormID(),
        //     targetingOrigin.x, targetingOrigin.y, targetingOrigin.z,
        //     targetPosition.x, targetPosition.y, targetPosition.z,
        //     horizontalDistance);

        return horizontalDistance;
    }
    
    struct ScaledArc {
        float duration;
        float apex;
    };

    static inline ScaledArc scaleArc(float horizontalDistance, float baseDuration, float baseApex) {
        constexpr float minimumDistance = 128.0f;
        constexpr float referenceDistance = 768.0f;
        horizontalDistance = std::max(horizontalDistance, minimumDistance);

        if (horizontalDistance <= referenceDistance) {
            const float normalized = (horizontalDistance - minimumDistance) / (referenceDistance - minimumDistance);
            const float smooth = normalized * normalized* (3.0f - 2.0f * normalized);
            constexpr float minimumDurationScale = 0.70f;
            constexpr float minimumApexScale = 0.95f;
            return {
                .duration = std::lerp(baseDuration * minimumDurationScale, baseDuration, smooth),
                .apex = std::lerp(baseApex * minimumApexScale, baseApex, smooth)
            };
        }

        //slight adjust of apex. more pronounced scaling of flight duration
        const float distanceDoublings = std::log2(horizontalDistance / referenceDistance);
        constexpr float durationGrowthPerDoubling = 0.25f; //+25% increase in base flight duration per doubling of forward dist
        constexpr float apexGrowthPerDoubling = 0.15f; //15% increase of base apex height per doubling of forward dist

        return {
            .duration = std::min(baseDuration * (1.0f+durationGrowthPerDoubling * distanceDoublings), baseDuration * 2.0f),
            .apex = std::min(baseApex * (1.0f + apexGrowthPerDoubling * distanceDoublings), baseApex * 1.5f)
        };
    }

}
