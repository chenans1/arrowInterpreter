#pragma once

namespace arrow {
    class ProcessEventHook {
    public:
        static void Install();
        static void InstallProjectileHook();
        static void AddPendingArrow(const RE::Projectile* a_projectile, float a_offset);
    private:
        static RE::BSEventNotifyControl ProcessEvent_NPC(
            RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_sink,
            RE::BSAnimationGraphEvent* a_event,
            RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource);

        static RE::BSEventNotifyControl ProcessEvent_PC(
            RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_sink,
            RE::BSAnimationGraphEvent* a_event,
            RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource);

        static inline REL::Relocation<decltype(ProcessEvent_NPC)> _originalNPC;
        static inline REL::Relocation<decltype(ProcessEvent_PC)> _originalPC;

        //used to detect the arrows that are fired. 
        static inline std::unordered_map<const RE::Projectile*, float> pendingArrows;
        static inline std::mutex pendingArrowsMutex;

        static void InitProjectile(RE::Projectile* a_this);
        static inline REL::Relocation<decltype(InitProjectile)> _InitProjectile;

    };
}