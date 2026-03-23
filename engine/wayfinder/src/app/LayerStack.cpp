#include "LayerStack.h"

#include <algorithm>

namespace Wayfinder
{

    LayerStack::~LayerStack()
    {
        for (auto& layer : m_layers)
        {
            layer->OnDetach();
        }
    }

    void LayerStack::PushLayer(std::unique_ptr<Layer> layer)
    {
        layer->OnAttach();
        m_layers.emplace(m_layers.begin() + m_layerInsertIndex, std::move(layer));
        m_layerInsertIndex++;
    }

    void LayerStack::PushOverlay(std::unique_ptr<Layer> overlay)
    {
        overlay->OnAttach();
        m_layers.emplace_back(std::move(overlay));
    }

    void LayerStack::PopLayer(Layer* layer)
    {
        auto it = std::find_if(m_layers.begin(), m_layers.begin() + m_layerInsertIndex, [layer](const std::unique_ptr<Layer>& l) { return l.get() == layer; });

        if (it != m_layers.begin() + m_layerInsertIndex)
        {
            (*it)->OnDetach();
            m_layers.erase(it);
            m_layerInsertIndex--;
        }
    }

    void LayerStack::PopOverlay(Layer* overlay)
    {
        auto it = std::find_if(m_layers.begin() + m_layerInsertIndex, m_layers.end(), [overlay](const std::unique_ptr<Layer>& l) { return l.get() == overlay; });

        if (it != m_layers.end())
        {
            (*it)->OnDetach();
            m_layers.erase(it);
        }
    }

}