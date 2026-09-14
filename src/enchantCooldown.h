#pragma once

namespace EnchantCooldown {
    inline RE::SpellItem* cooldownSpell = nullptr;       // 0x800
    inline RE::EffectSetting* cooldownEffect = nullptr;  // 0x801

    inline bool LoadForms() {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();

        cooldownSpell = dataHandler->LookupForm<RE::SpellItem>(0x800, "ArrowInterpreter.esp");

        cooldownEffect = dataHandler->LookupForm<RE::EffectSetting>(0x801, "ArrowInterpreter.esp");

        if (!cooldownSpell || !cooldownEffect) {
            SKSE::log::error("Failed to load enchant cooldown forms: spell={}, effect={}", static_cast<void*>(cooldownSpell), static_cast<void*>(cooldownEffect));
            return false;
        }
        SKSE::log::info("Sucessfully loaded enchant cd forms: spell={}, effect={}", static_cast<void*>(cooldownSpell), static_cast<void*>(cooldownEffect));
        return true;
    }

    inline bool applyCD(RE::Actor* actor) {
        if (!actor || !cooldownSpell || !cooldownEffect) {
            return false;
        }
        auto* magicTarget = actor->GetMagicTarget();
        if (!magicTarget) {
            return false;
        }

        if (magicTarget->HasMagicEffect(cooldownEffect)) {
            return false;
        }

        if (auto* caster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant)) {
            // SKSE::log::info("[EnchantCooldown] applying cooldown spell");
            caster->CastSpellImmediate(cooldownSpell, true, actor, 1.0f, false, 0.0f, actor);
            return true;

        }
        return false;
    }
}