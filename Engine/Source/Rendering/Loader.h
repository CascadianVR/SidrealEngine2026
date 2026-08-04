#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "Rendering/RendererTypes.h"
#include "tiny_gltf_v3.h"
#include "stb_image.h"

class Loader {
public:
	static void LoadScene(const std::string& fileName);
	static std::vector<Model>& GetLoadedModels() { return m_loadedModels; }
	static std::vector<Texture>& GetLoadedTextures() { return m_loadedTextures; }

private:
	static void LoadFallbackTexture(const std::string& fileName);
	static void LoadGLB(const std::string& fileName, glm::mat4& modelMatrix);
	static void GetVertexData(const tg3_model& model, const tg3_primitive& primitive, Mesh& mesh);
	static void GetIndexData(const tg3_model& model, uint32_t accessorIndex, Mesh& mesh);
	static void GetTextureData(const tg3_model& model, const tg3_primitive& primitive, Mesh& mesh, size_t textureOffset);
	static int GetAccessorIndex(const tg3_primitive &primitive, const char* accessorName);

	static inline std::vector<Model> m_loadedModels;
	static inline std::unordered_map<std::string, uint32_t> m_modelLookup;
	static inline std::vector<Texture> m_loadedTextures;
	static inline std::unordered_map<std::string, uint32_t> m_textureLookup;
};