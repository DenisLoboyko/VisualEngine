#pragma once
#include "../Physics/CharacterController.h"

namespace VE {
    struct CharacterControllerComponent {
        CharacterController controller;
        int targetObjectIndex = -1; // индекс объекта в objects[]
    };
}
