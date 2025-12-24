#include "Includes/Bindless.hlsl"
#include "Includes/Types.hlsl"

#define MAX_VERTICES  64
#define MAX_TRIANGLES 64

struct SRMeshlet {
	u32 vertexOffset;
	u32 triangleOffset;
	u32 vertexCount;
	u32 triangleCount;
};

struct SRVertex {
	float3 pos;
	//f32 pad;
};

struct MSOutput {
	float4 pos   : SV_Position;
	float3 color : COLOR;
};

struct Primitive {
	bool cull: SV_CullPrimitive;
};

struct PushConstants {
	SRDescriptorIndex vertexBufferIdx;
	SRDescriptorIndex meshletBufferIdx;
	SRDescriptorIndex meshletVerticesBufferIdx;
	SRDescriptorIndex meshletTrianglesBufferIdx;
};

SR_PUSH_CONSTANT(PushConstants, g_Push);

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(MAX_VERTICES, 1, 1)]
void meshMain(
	in uint3 globalThreadID : SV_DispatchThreadID,
	in uint3 localThreadID : SV_GroupThreadID,
	in uint3 groupID : SV_GroupID,
	out vertices MSOutput emitVertices[MAX_VERTICES],
	out indices u32vec3 emitTriangles[MAX_TRIANGLES]
) {
	// TODO: For now we pack 3 u8s into a u32 for alignment reasons, but
	// it would probably better to pack this without padding byte.
	StructuredBuffer<SRVertex> vertexBuffer = ResourceDescriptorHeap[g_Push.vertexBufferIdx];
	StructuredBuffer<SRMeshlet> meshletBuffer = ResourceDescriptorHeap[g_Push.meshletBufferIdx];
	StructuredBuffer<u32> meshletVerticesBuffer = ResourceDescriptorHeap[g_Push.meshletVerticesBufferIdx];
	StructuredBuffer<u32> meshletTrianglesBuffer = ResourceDescriptorHeap[g_Push.meshletTrianglesBufferIdx];

	SRMeshlet meshlet = meshletBuffer[groupID.x];
	SetMeshOutputCounts(meshlet.vertexCount, meshlet.triangleCount);

	if (localThreadID.x < meshlet.triangleCount) {
		u32 packed = meshletTrianglesBuffer[meshlet.triangleOffset + localThreadID.x];
		emitTriangles[localThreadID.x] = u32vec3(
			(packed >>  0) & 0xFF,
			(packed >>  8) & 0xFF,
			(packed >> 16) & 0xFF
		);
	}

	if (localThreadID.x < meshlet.vertexCount) {
		u32 vertexIdx = meshlet.vertexOffset + localThreadID.x;
		vertexIdx = meshletVerticesBuffer[vertexIdx];

		SRVertex vertex = vertexBuffer[vertexIdx];
		float4 outPosition;
		outPosition = mul(g_PerFrameData.view, float4(vertex.pos, 1.0f));
		outPosition = mul(g_PerFrameData.proj, outPosition);

		float3 outColor = float3(
            float(groupID.x & 1),
            float(groupID.x & 3) / 4,
            float(groupID.x & 7) / 8
		);

		emitVertices[localThreadID.x].pos = outPosition;
		emitVertices[localThreadID.x].color = outColor;
	}
}

[shader("pixel")]
float4 pixelMain(MSOutput input) : SV_Target {
	return float4(input.color, 1.0f);
}
