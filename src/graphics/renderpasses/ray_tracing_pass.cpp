#include "ray_tracing_pass.h"

RayTracingPass::RayTracingPass(GFXDevice& gfxDevice, Scene& scene) : m_GfxDevice(gfxDevice) {

	const auto& entities = scene.get_entities();
	size_t numBLASes = 0;

	// Count number of BLASes required
	for (const auto& entity : entities) {
		const Renderable* renderable = ecs::get_component_renderable(entity);
		const Model* model = renderable->model;

		for (const auto& mesh : model->meshes) {
			numBLASes += mesh.primitives.size();
		}
	}
	m_BLASes.reserve(numBLASes);

	for (const auto& entity : entities) {
		const Renderable* renderable = ecs::get_component_renderable(entity);
		const Model* model = renderable->model;

		for (const auto& mesh : model->meshes) {
			for (const auto& primitive : mesh.primitives) { // TODO: Rename MeshPrimitive to just "Mesh", GLTF terminology is confusing
				RTAS& blas = m_BLASes.emplace_back();

				const RTASInfo blasInfo = {
					.type = RTASType::BLAS,
					.blas = {
						.geometries = {
							RTBLASGeometry {
								.type = RTBLASGeometry::Type::TRIANGLES,
								.triangles = {
									.vertexBuffer = &model->vertexBuffer,
									.indexBuffer = &model->indexBuffer,
									.vertexFormat = Format::R32G32B32_FLOAT,
									.vertexCount = primitive.numVertices,
									.vertexStride = sizeof(ModelVertex),
									.vertexByteOffset = sizeof(ModelVertex) * primitive.baseVertex,
									.indexCount = primitive.numIndices,
									.indexOffset = primitive.baseIndex
								}
							}
						}
					}
				};

				m_GfxDevice.create_rtas(blasInfo, blas);
			}
		}
	}
}

void RayTracingPass::execute(PassExecuteInfo& executeInfo, Scene& scene) {

}
