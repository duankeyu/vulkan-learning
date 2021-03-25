#version 450

layout (location = 0) in vec2 uv;

layout (binding = 0) uniform sampler2D origin_image;
layout (binding = 1) uniform sampler2D blur_image;

layout (location = 0) out vec4 color;

void main()
{
	vec3 hdrColor = texture(origin_image, uv).rgb;
	vec3 bloomColor = texture(blur_image, uv).rgb;
	hdrColor += bloomColor;
	vec3 result = vec3(1.0) - exp(-hdrColor);
	result = pow(result, vec3(1.0 / 2.2));
	color = vec4(result, 1.0f);
}