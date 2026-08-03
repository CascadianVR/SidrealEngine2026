#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "Rendering/RendererTypes.h"
#include "tiny_gltf_v3.h"

class Loader {
public:
	static std::vector<Model> LoadScene(const std::string& fileName);

private:
	static void LoadGLB(const std::string& fileName, glm::mat4& modelMatrix);
	static void GetVertexData(const tg3_model& model, const tg3_primitive& primitive, Mesh& mesh);
	static void GetIndexData(const tg3_model& model, uint32_t accessorIndex, Mesh& mesh);
	static int GetAccessorIndex(const tg3_primitive &primitive, const char* accessorName);

	static inline std::vector<Model> m_loadedModels;
	static inline std::unordered_map<std::string, uint32_t> m_modelAssetLookup;
};