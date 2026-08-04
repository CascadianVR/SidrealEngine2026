#pragma once

#include <unordered_set>
#include "RendererTypes.h"

class Primitives
{
public:
    static void GetPrimitive(const uint32_t id, Model& model)
    {
        switch (id)
        {
            case 0: CreateCube(model); break;
            case 1: CreateQuad(model); break;
            default: ;
        }
    }
    static void CreateCube(Model& model);
    static void CreateQuad(Model& model);
    
    static inline std::unordered_map<std::string, uint32_t> string_map = {
        {"Cube", 0}, {"Quad", 1}
    };
};
