#pragma once
#include "context.h"
#include "mesh.h"
#include "shader.h"


template <typename T>
class ResourceManager {
public:
    static ResourceManager& getInstance() {
        static ResourceManager resourceManager;
        return resourceManager;
    }

    bool add(const std::string& id, T resource) {
        m_resources[id] = resource;
        return true;
    }

    T get(const std::string& id) {
        return m_resources[id];
    }

    bool remove(const std::string& id) {
        auto it = m_resources.find(id);
        if (it != m_resources.end()) {
            m_resources.erase(it);
            return true;
        }
        return false;
    }

    void clear() {
        m_resources.clear();
    }

    std::unordered_map<std::string, T>& getAll() {
        return m_resources;
    }

private:
    std::unordered_map<std::string, T> m_resources{};
};

using TextureManager = ResourceManager<TextureView>;
using SamplerManager = ResourceManager<Sampler>;