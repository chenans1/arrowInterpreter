#include "PCH.h"
#include "hitDataAPI.h"
#include <cstddef>
#include "processEventHook.h"

using namespace SKSE;
using namespace SKSE::log;
using namespace std::literals;

namespace arrow
{
	namespace
	{
		using HitDataConstructor = RE::HitData* (*)(RE::HitData*);
		using InitializeArrowData = void (*)(RE::HitData*, RE::Actor*, RE::Actor*, RE::Projectile*);
		using ApplyHitData = void (*)(RE::Actor*, RE::HitData*);
		using HitDataDestructor = void (*)(RE::HitData*);

		REL::Relocation<HitDataConstructor> constructHitData{ RELOCATION_ID(42826, 43995) };
		REL::Relocation<HitDataDestructor> destroyHitData{ RELOCATION_ID(42828, 43997) };
		REL::Relocation<InitializeArrowData> initializeArrowData{ RELOCATION_ID(42833, 44002) };
		REL::Relocation<ApplyHitData> applyHitData{ REL::ID(38586) }; //need to be double checked?
	}

	bool ApplyArrowHit(RE::Projectile* a_projectile, RE::Actor* a_target)
	{
		if (!a_projectile || !a_target) {
			return false;
		}
		
		alignas(RE::HitData) std::byte storage[sizeof(RE::HitData)];
		auto* hitData = reinterpret_cast<RE::HitData*>(storage);
		constructHitData(hitData);
		
		//The vanilla projectile path passes a null aggressor here. The initializer recovers the shooter, weapon and damage from the projectile.
		initializeArrowData(hitData, nullptr, a_target, a_projectile);
		// log::info("[hitDataAPI] Initialized hitData");
		applyHitData(a_target, hitData);
		destroyHitData(hitData);
		// log::info("[hitDataAPI] Applied HitData");
		// a_projectile->Kill();
		return true;
	}
}
