#include "Includes/Bindless.hlsl"

const static float2 positions[3] = {
	float2(0.0, -0.5),
	float2(0.5, 0.5),
	float2(-0.5, 0.5)
};

struct Vertex {
	float4 pos : SV_Position;
};

struct Primitive {
	bool cull: SV_CullPrimitive;
};

#define MAX_VERTS 3
#define MAX_PRIMS 1

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(MAX_VERTS, 1, 1)]
void meshMain(
	in uint tig : SV_GroupIndex,
	OutputVertices<Vertex, MAX_VERTS> outVertices,
	OutputIndices<uint3, MAX_PRIMS> outIndices,
	OutputPrimitives<Primitive, MAX_PRIMS> outPrimitives
) {
	const uint numVertices = 3;
	const uint numPrimitives = 1;
	SetMeshOutputCounts(numVertices, numPrimitives);
}

[shader("pixel")]
float4 pixelMain() {
	return float4(1.0, 0.0, 0.0, 1.0);
}
