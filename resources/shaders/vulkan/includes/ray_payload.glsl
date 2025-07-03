struct RayPayload {
	vec3 color;
	float distance;
	vec3 scatterDir;
	bool isScattered;
	uint rngSeed;
	vec3 incomingLight;
};