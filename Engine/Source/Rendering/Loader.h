#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "Rendering/RendererTypes.h"
#include "tiny_gltf_v3.h"

class Loader {
public:
	static void LoadGLB(const std::string& fileName);
	static std::unordered_map<std::string, Model>& GetLoadedModels() { return loadedModels; }
private:
	static void GetVertexData(const tg3_model& model, const tg3_primitive& primitive, std::vector<Vertex>& vertices);
	static void GetIndexData(const tg3_model& model, uint32_t accessorIndex, std::vector<uint32_t>& indices);
	static void CreateVertexBuffer(Mesh& mesh);
	static void CreateIndexBuffer(Mesh& mesh);
	static int GetAccessorIndex(const tg3_primitive &primitive, const char* accessorName);
	static std::unordered_map<std::string, Model> loadedModels;
};