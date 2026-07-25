#include "PCH.h"
#include "payload.h"

namespace arrow {
    //helpers for trimming strings

    arrowPayload process(std::string_view payload) { 
        arrowPayload K;
        K.arrowCount = 1;
        K.damageMult = 1.0f;
        K.spreadAngle = 1.0f;
        return K;
    }
}