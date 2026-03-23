#pragma once

#include "core/events/Event.h"

#include <string>

namespace Wayfinder
{

    class Layer
    {
    public:
        virtual ~Layer() = default;

        virtual void OnAttach() {}
        virtual void OnDetach() {}
        virtual void OnUpdate(float /*deltaTime*/) {}
        virtual void OnEvent(Event& /*event*/) {}

        virtual const char* GetName() const
        {
            return "Layer";
        }
    };

}