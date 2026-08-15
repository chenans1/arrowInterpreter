#pragma once

namespace draugr {
    class ProcessEventHook {
    public:
        static void Install();

    private:
        //static RE::BSEventNotifyControl ProcessEvent_NPC(
        //    RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_sink,
        //    RE::BSAnimationGraphEvent* a_event,
        //    RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource);

        //static RE::BSEventNotifyControl ProcessEvent_PC(
        //    RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_sink,
        //    RE::BSAnimationGraphEvent* a_event,
        //    RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource);

        //static inline REL::Relocation<decltype(ProcessEvent_NPC)> _originalNPC;
        //static inline REL::Relocation<decltype(ProcessEvent_PC)> _originalPC;

        static bool NotifyAnimationGraph_NPC(RE::IAnimationGraphManagerHolder* a_this, const RE::BSFixedString& a_eventName);

        static inline REL::Relocation<decltype(NotifyAnimationGraph_NPC)> _originalNPC;
    };
}