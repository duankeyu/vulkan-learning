#version 450

layout (location = 0, binding = 0) uniform sampler2D inputImage;
layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 color;

float calculateBrightness(vec3 rgb)
{
	return 0.2126 * rgb.r + 0.7152 * rgb.g + 0.0722 * rgb.b;
}

void main()
{
	vec4 rgba = texture(inputImage, uv);
	if(calculateBrightness(rgba.rgb) < 0.5)
		discard;
	color = rgba;
}