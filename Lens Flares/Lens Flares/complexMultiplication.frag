#version 450

layout (location = 0) in vec2 uv;

layout (binding = 0) uniform sampler2D sampler_real_1;
layout (binding = 1) uniform sampler2D sampler_imaginary_1;
layout (binding = 2) uniform sampler2D sampler_real_2;
layout (binding = 3) uniform sampler2D sampler_imaginary_2;

layout (location = 0) out vec4 real;
layout (location = 1) out vec4 imaginary;

void main()
{
	vec4 real_1 = texture(sampler_real_1, uv);
	vec4 imaginary_1 = texture(sampler_imaginary_1, uv);
	vec4 real_2 = texture(sampler_real_2, uv);
	vec4 imaginary_2 = texture(sampler_imaginary_2, uv);
	real = real_1 * real_2 - imaginary_1 * imaginary_2;
	imaginary = real_1 * imaginary_2 + real_2 * imaginary_1;
}