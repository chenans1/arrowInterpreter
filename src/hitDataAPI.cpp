#include "PCH.h"
#include "hitDataAPI.h"
#include <cstddef>

namespace arrow
{
	namespace
	{
		using HitDataConstructor = RE::HitData* (*)(RE::HitData*);
		using InitializeArrowData = void (*)(RE::HitData*, RE::Actor*, RE::Actor*, RE::Projectile*);
		using ApplyHitData = void (*)(RE::Actor*, RE::HitData*);
		using HitDataDestructor = void (*)(RE::HitData*);

		// AE Address Library IDs
		REL::Relocation<HitDataConstructor> constructHitData{ REL::ID(43995) };
		REL::Relocation<HitDataDestructor> destroyHitData{ REL::ID(43997) };
		REL::Relocation<InitializeArrowData> initializeArrowData{ REL::ID(44002) };
		REL::Relocation<ApplyHitData> applyHitData{ REL::ID(38586) };
	}

	bool ApplyArrowHit(RE::Projectile* a_projectile, RE::Actor* a_target)
	{
		if (!a_projectile || !a_target) {
			return false;
		}

		if (!REL::Module::IsAE()) {
			SKSE::log::error("[hitDataAPI] ApplyArrowHit called on an unsupported non-AE runtime");
			return false;
		}

		alignas(RE::HitData) std::byte storage[sizeof(RE::HitData)];
		auto* hitData = reinterpret_cast<RE::HitData*>(storage);
		constructHitData(hitData);

		// The vanilla projectile path passes a null aggressor here. The initializer recovers the shooter, weapon and damage from the projectile.
		initializeArrowData(hitData, nullptr, a_target, a_projectile);
		applyHitData(a_target, hitData);
		destroyHitData(hitData);
		return true;
	}
}
