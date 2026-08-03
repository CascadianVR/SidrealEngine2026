#define TINYGLTF3_IMPLEMENTATION
#include "Loader.h"

#include <fstream>
#include <filesystem>
#include <glm/ext/matrix_transform.hpp>

#include "Logger.h"
#include "json.hpp"

using json = nlohmann::json;

std::vector<Model> Loader::LoadScene(const std::string& fileName)
{
	m_loadedModels.clear();
	m_modelAssetLookup.clear();

	// Load json scene file
	std::ifstream file(fileName);
	if (!file)
	{
		Logger::Error("Failed to open file: ", fileName);
		return {};
	}

	json data = json::parse(file);
	
	Logger::Info(data.dump(2));
	
	if (!data.contains("sceneData") || !data["sceneData"].is_array())
	{
		Logger::Error("Scene file is missing a valid \"sceneData\" array.");
		return {};
	}
	
	// Get the "sceneData" object and get each entry
	const json& sceneData = data["sceneData"];

	glm::mat4 modelMatrix;
	for (const json& sceneObject : sceneData)
	{
		if (!sceneObject.is_object())
		{
			Logger::Warn("Skipping non-object sceneData entry: ", sceneObject.dump());
			continue;
		}

		// Model path
		if (!sceneObject.contains("path") || !sceneObject["path"].is_string())
		{
			Logger::Warn("Skipping scene object without valid \"path\" field: ", sceneObject.dump());
			continue;
		}
		const std::string modelPath = sceneObject["path"].get<std::string>();

		// Model position
		if (!sceneObject.contains("position") || !sceneObject["position"].is_array() || sceneObject["position"].size() != 3)
		{
			Logger::Warn("Skipping scene object without valid \"position\" field: ", sceneObject.dump());
			continue;
		}
		const json& position = sceneObject["position"];
		glm::vec3 modelPosition = { position[0].get<float>(),position[1].get<float>(),position[2].get<float>() };

		// Model rotation
		if (!sceneObject.contains("rotation") || !sceneObject["rotation"].is_array() || sceneObject["rotation"].size() != 3)
		{
			Logger::Warn("Skipping scene object without valid \"rotation\" field: ", sceneObject.dump());
			continue;
		}
		const json& rotation = sceneObject["rotation"];
		glm::vec3 modelRotation = { rotation[0].get<float>(),rotation[1].get<float>(),rotation[2].get<float>() };

		// Model scale
		if (!sceneObject.contains("scale") || !sceneObject["scale"].is_array() || sceneObject["scale"].size() != 3)
		{
			Logger::Warn("Skipping scene object without valid \"scale\" field: ", sceneObject.dump());
			continue;
		}
		const json& scale = sceneObject["scale"];
		glm::vec3 modelScale = { scale[0].get<float>(),scale[1].get<float>(),scale[2].get<float>() };

		// Construct model matrix
		modelMatrix = glm::translate(glm::mat4(1.0f), modelPosition);
		modelMatrix = glm::rotate(modelMatrix, glm::radians(modelRotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		modelMatrix = glm::rotate(modelMatrix, glm::radians(modelRotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		modelMatrix = glm::rotate(modelMatrix, glm::radians(modelRotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
		modelMatrix = glm::scale(modelMatrix, modelScale);

		Logger::Info("Loading model: ", modelPath);
		LoadGLB(modelPath, modelMatrix);
	}

	for (int i = 0; i < 10; i++)
	{
		for (int j = 0; j < 10; j++)
		{
			for (int k = 0; k < 10; k++)
			{
				glm::vec3 position = { static_cast<float>(i) * 1.0f, static_cast<float>(j) * 1.7f, static_cast<float>(k) * -1.0f };
				glm::vec3 rotation = { 0.0f, 0.0f, 0.0f };
				glm::vec3 scale = { 1.0f, 1.0f, 1.0f };
				modelMatrix = glm::translate(glm::mat4(1.0f), position);
				modelMatrix = glm::rotate(modelMatrix, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
				modelMatrix = glm::rotate(modelMatrix, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
				modelMatrix = glm::rotate(modelMatrix, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
				modelMatrix = glm::scale(modelMatrix, scale);
				LoadGLB("Resources\\Models\\Cascadia.glb", modelMatrix);
			}
		}
	}

	for (int i = 0; i < 10; i++)
	{
		for (int j = 0; j < 10; j++)
		{
			for (int k = 0; k < 10; k++)
			{
				glm::vec3 position = { -static_cast<float>(i) * 1.0f - 3.0f, static_cast<float>(j) * 1.7f, static_cast<float>(k) * -1.0f };
				glm::vec3 rotation = { 0.0f, 0.0f, 0.0f };
				glm::vec3 scale = { 1.0f, 1.0f, 1.0f };
				modelMatrix = glm::translate(glm::mat4(1.0f), position);
				modelMatrix = glm::rotate(modelMatrix, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
				modelMatrix = glm::rotate(modelMatrix, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
				modelMatrix = glm::rotate(modelMatrix, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
				modelMatrix = glm::scale(modelMatrix, scale);
				LoadGLB("Resources\\Models\\ThickBase5.glb", modelMatrix);
			}
		}
	}

	return m_loadedModels;
}

void Loader::LoadGLB(const std::string& fileName, glm::mat4& modelMatrix)
{
	if (m_modelAssetLookup.find(fileName) != m_modelAssetLookup.end())
	{
		//Logger::Warn("Model '", fileName, "' already loaded, adding instanced version.");
		
		Model& model = m_loadedModels[m_modelAssetLookup[fileName]];
		model.instanceCount += 1;
		model.instanceMatrices.push_back(modelMatrix);
		
		return;
	}
	
	// Load the GLB file into memory
	std::ifstream file(fileName, std::ios::binary | std::ios::ate);
	if (!file)
	{
		Logger::Warn("Failed to open file: ", fileName);
		return;
	}
	
	Logger::Info("Loading GLB file: ", fileName);

	std::streamsize size = file.tellg();
	if (size < 0)
	{
		Logger::Error("Failed to get file size: ", fileName);
		return;
	}

	std::vector<uint8_t> data(static_cast<size_t>(size));
	file.seekg(0, std::ios::beg);
	if (!file.read(
		reinterpret_cast<char*>(data.data()),
		size))
	{
		Logger::Error("Failed to read file: ", fileName);
		return;
	}

	// Parse the GLB data using TinyGLTF3
	tg3_parse_options opts;
	tg3_error_stack errors;
	tg3_model gltfModel;

	tg3_parse_options_init(&opts);
	tg3_error_stack_init(&errors);

	tg3_error_code err = tg3_parse_glb(
		&gltfModel,
		&errors,
		data.data(),
		data.size(),
		std::filesystem::current_path().generic_string().c_str(),
		static_cast<uint32_t>(std::filesystem::current_path().generic_string().size()),
		&opts
	);

	if (err != 0)
	{
		Logger::Error("Failed to parse GLB file: ", fileName);
		return;
	}


	//Logger::Info("Model has ", gltfModel.meshes_count, " meshes.");
	Model model{};
	for (uint32_t i = 0; i < gltfModel.meshes_count; ++i)
	{
		//Logger::Info("Processing mesh ", i, ": ", gltfModel.meshes[i].name.data);
		const tg3_mesh& gltfMesh = gltfModel.meshes[i];
		for (uint32_t j = 0; j < gltfMesh.primitives_count; ++j)
		{

			//Logger::Info("Processing primitive ", j, " of mesh ", i);
			const tg3_primitive& primitive = gltfMesh.primitives[j];

			// List all attributes of the primitive
			//Logger::Info("Primitive ", j, " has ", primitive.attributes_count, " attributes.");
			//for (uint32_t k = 0; k < primitive.attributes_count; ++k)
			//{
			//	const tg3_str_int_pair& attribute = primitive.attributes[k];
			//	Logger::Info("  Attribute ", k, ": Name: ", attribute.key.data, ", Accessor Index: ", attribute.value);
			//}
			
			Mesh mesh;
			GetVertexData(gltfModel, primitive, mesh);
			GetIndexData(gltfModel, primitive.indices, mesh);

			model.meshes.push_back(mesh);
		}
	}

	model.name = fileName;
	model.instanceMatrices[0] = modelMatrix;
	m_loadedModels.push_back(model);
	m_modelAssetLookup[fileName] = static_cast<uint32_t>(m_loadedModels.size() - 1);
	
	Logger::Success("Loaded GLB file: ", fileName);
}

void Loader::GetVertexData(const tg3_model& model, const tg3_primitive& primitive, Mesh& mesh)
{
	int positionAccessorIndex = GetAccessorIndex(primitive, "POSITION");
	int normalAccessorIndex = GetAccessorIndex(primitive, "NORMAL");
	int uvAccessorIndex = GetAccessorIndex(primitive, "TEXCOORD_0");

	// Position data
	const tg3_accessor& positionAccessor = model.accessors[positionAccessorIndex];
	const tg3_buffer_view& positionBufferView = model.buffer_views[positionAccessor.buffer_view];
	const tg3_buffer& positionBuffer = model.buffers[positionBufferView.buffer];
	const uint8_t* positionData = positionBuffer.data.data + positionBufferView.byte_offset + positionAccessor.byte_offset;
	const uint32_t positionStride = positionBufferView.byte_stride != 0 ? positionBufferView.byte_stride : static_cast<uint32_t>(sizeof(glm::vec3));
	
	// Normal data
	const tg3_accessor& normalAccessor = model.accessors[normalAccessorIndex];
	const tg3_buffer_view& normalBufferView = model.buffer_views[normalAccessor.buffer_view];
	const tg3_buffer& normalBuffer = model.buffers[normalBufferView.buffer];
	const uint8_t* normalData = normalBuffer.data.data + normalBufferView.byte_offset + normalAccessor.byte_offset;
	const uint32_t normalStride = normalBufferView.byte_stride != 0 ? normalBufferView.byte_stride : static_cast<uint32_t>(sizeof(glm::vec3));
	
	// UV data
	const tg3_accessor& uvAccessor = model.accessors[uvAccessorIndex];
	const tg3_buffer_view& uvBufferView = model.buffer_views[uvAccessor.buffer_view];
	const tg3_buffer& uvBuffer = model.buffers[uvBufferView.buffer];
	const uint8_t* uvData = uvBuffer.data.data + uvBufferView.byte_offset + uvAccessor.byte_offset;
	const uint32_t uvStride = uvBufferView.byte_stride != 0 ? uvBufferView.byte_stride : static_cast<uint32_t>(sizeof(glm::vec2));

	mesh.vertices.reserve(positionAccessor.count);
	
	// Assuming the accessor type is VEC3 and component type is FLOAT
	for (uint32_t v = 0; v < positionAccessor.count; v++)
	{
		glm::vec4 position{ 1.0f };
		std::memcpy(&position, positionStride * v + positionData, sizeof(glm::vec3));
		position.w = 1.0f;

		glm::vec4 normal{ 0.0f };
		std::memcpy(&normal, normalStride * v + normalData, sizeof(glm::vec3));

		glm::vec4 uv{ 0.0f };
		std::memcpy(&uv, uvStride * v + uvData, sizeof(glm::vec2));

		// Initialize normal and uv to sensible defaults to avoid uninitialized memory
		Vertex vert{ position, normal, uv };
		mesh.vertices.push_back(vert);
	}

	mesh.vertexCount = static_cast<uint32_t>(mesh.vertices.size());
}

void Loader::GetIndexData(const tg3_model& model, const uint32_t accessorIndex, Mesh& mesh) {
	// Load vertex positions from the first accessor of the first primitive
	const tg3_accessor& accessor = model.accessors[accessorIndex];
	const tg3_buffer_view& bufferView = model.buffer_views[accessor.buffer_view];
	const tg3_buffer& buffer = model.buffers[bufferView.buffer];

	const uint8_t* data = buffer.data.data + bufferView.byte_offset + accessor.byte_offset;

	mesh.indices.resize(accessor.count);

	//Logger::Info("Index type: ", accessor.component_type);
	switch (accessor.component_type)
	{
		case TG3_COMPONENT_TYPE_UNSIGNED_BYTE: {
			const uint8_t* src = data;
			for (uint32_t i = 0; i < accessor.count; i++) mesh.indices[i] = src[i];
			break;
		}
		case TG3_COMPONENT_TYPE_UNSIGNED_SHORT: {
			const uint16_t* src = reinterpret_cast<const uint16_t*>(data);
			for (uint32_t i = 0; i < accessor.count; i++) mesh.indices[i] = src[i];
			break;
		}
		case TG3_COMPONENT_TYPE_UNSIGNED_INT: {
			const uint32_t* src = reinterpret_cast<const uint32_t*>(data);
			for (uint32_t i = 0; i < accessor.count; i++) mesh.indices[i] = src[i];
			break;
		}
		default:{
			Logger::Error("Unsupported index component type.");
			mesh.indices.clear();
			break;
		}
	}

	mesh.indexCount = static_cast<uint32_t>(mesh.indices.size());
}

int Loader::GetAccessorIndex(const tg3_primitive &primitive, const char* accessorName)
{
	int accessorIndex = 999;
	for (uint32_t a = 0; a < primitive.attributes_count; ++a) {
		const tg3_str_int_pair& attribute = primitive.attributes[a];
		if (std::string(attribute.key.data) == accessorName) {
			accessorIndex = attribute.value;
			break;
		}
	}

	if (accessorIndex == 999)
	{
		Logger::Error("[Loader] ", accessorName, " Accessor Index not found!");
	}

	return accessorIndex;
}
