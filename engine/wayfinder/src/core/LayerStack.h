#pragma once

#include "core/Layer.h"

#include <memory>
#include <vector>

namespace Wayfinder
{

    class LayerStack
    {
    public:
        LayerStack() = default;
        ~LayerStack();

        void PushLayer(std::unique_ptr<Layer> layer);
        void PushOverlay(std::unique_ptr<Layer> overlay);
        void PopLayer(Layer* layer);
        void PopOverlay(Layer* overlay);

        using Iterator = std::vector<std::unique_ptr<Layer>>::iterator;
        using ConstIterator = std::vector<std::unique_ptr<Layer>>::const_iterator;
        using ReverseIterator = std::vector<std::unique_ptr<Layer>>::reverse_iterator;
        using ConstReverseIterator = std::vector<std::unique_ptr<Layer>>::const_reverse_iterator;

        Iterator begin() { return m_layers.begin(); }
        Iterator end() { return m_layers.end(); }
        ReverseIterator rbegin() { return m_layers.rbegin(); }
        ReverseIterator rend() { return m_layers.rend(); }

        ConstIterator begin() const { return m_layers.begin(); }
        ConstIterator end() const { return m_layers.end(); }
        ConstReverseIterator rbegin() const { return m_layers.rbegin(); }
        ConstReverseIterator rend() const { return m_layers.rend(); }

    private:
        std::vector<std::unique_ptr<Layer>> m_layers;
        unsigned int m_layerInsertIndex = 0;
    };

}