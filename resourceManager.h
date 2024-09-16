#pragma once
#include "context.h"

template <typename T>
class ResourceManager {
public:
    static ResourceManager& getInstance() {
        static ResourceManager resourceManager;
        return resourceManager;
    }

    bool add(const std::string& id, T resource) {
        if (has(id)) {
            throw std::runtime_error("Resource with ID " + id + " already exists.");
        }
        m_resources[id] = resource;
        return true;
    }

    const T& get(const std::string& id) {
        if (!has(id)) {
            throw std::runtime_error("Resource with ID " + id + " does not exist.");
        }
        return m_resources[id];
    }

    bool has(const std::string& id) {
        return m_resources.find(id) != m_resources.end();
    }

    bool remove(const std::string& id) {
        auto it = m_resources.find(id);
        if (it != m_resources.end()) {
            m_resources.erase(it);
            return true;
        }
        throw std::runtime_error("Resource with ID " + id + " does not exist.");
    }

    void clear() {
        m_resources.clear();
    }

    std::unordered_map<std::string, T>& getAll() {
        return m_resources;
    }

protected:
    std::unordered_map<std::string, T> m_resources{};
};

using TextureManager = ResourceManager<TextureView>;
using SamplerManager = ResourceManager<Sampler>;