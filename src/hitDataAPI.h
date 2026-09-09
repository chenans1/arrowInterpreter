#pragma once

namespace arrow
{
	// Applies a real engine arrow hit to a_target using all source information carried by a_projectile.
	[[nodiscard]] bool ApplyArrowHit(RE::Projectile* a_projectile, RE::Actor* a_target);
}
