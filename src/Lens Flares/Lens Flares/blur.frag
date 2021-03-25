#version 450

layout (location = 0, binding = 0) uniform sampler2D inputImage;
layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 color;

layout (binding = 0) uniform Delta
{
	float u;
	float v;
} delta;

void main()
{
	float u0 = uv.x - delta.u >= 0.0 ? uv.x - delta.u : 1.0 - delta.u;
	float u1 = uv.x + delta.u <= 1.0 ? uv.x + delta.u : delta.u;
	float v0 = uv.y - delta.v >= 0.0 ? uv.y - delta.v : 1.0 - delta.v;
	float v1 = uv.y + delta.v <= 1.0 ? uv.y + delta.v : delta.v;

	color = 0.075 * texture(inputImage, vec2(u0, v0)) + 
			0.124 * texture(inputImage, vec2(uv.x, v0)) + 
			0.075 * texture(inputImage, vec2(u1, v0)) + 
			0.124 * texture(inputImage, vec2(u0, uv.y)) + 
			0.204 * texture(inputImage, vec2(uv)) + 
			0.124 * texture(inputImage, vec2(u1, uv.y)) + 
			0.075 * texture(inputImage, vec2(u0, v1)) + 
			0.124 * texture(inputImage, vec2(uv.x, v1)) + 
			0.075 * texture(inputImage, vec2(u1, v1));
}