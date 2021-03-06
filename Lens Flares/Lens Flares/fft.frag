#version 450

layout (location = 0) in vec2 uv;

layout (binding = 0) uniform sampler2D sampler_0;
layout (binding = 1) uniform sampler2D sampler_1;

layout (binding = 2) uniform Delta
{
	float u;
	float v;
} delta;
layout (binding = 3) uniform bool inverse;

layout (location = 0) out vec4 atta_0;
layout (location = 1) out vec4 atta_1;

layout (constant_id = 0) const float PI = 3.14159265f;

float calculateBrightness(vec3 rgb)
{
	return 0.2126 * rgb.r + 0.7152 * rgb.g + 0.0722 * rgb.b;
}

void dft(vec2 uv, double width, double height, vec4 re, vec4 im)
{
	double temp = 0.0;

	for(float u = 0.0f; u <= 1.0f; u+=delta.u)
	{
		for(float v = 0.0f; v <= 1.0f; v+=delta.v)
		{
			temp = uv.u * u * height + uv.v * v * width;
			vec4 color = texture(sampler_0, vec2(u, v));
			re += color * cos(-2 * PI * temp);
			im += color * sin(-2 * PI * temp);
		}
	}
}

void idft(vec2 uv, double width, double height, vec4 real)
{
	double temp;

	for(float u = 0.0f; u <= 1.0f; u += delta.u)
	{
		for(float v = 0.0f; v <= 1.0f; v += delta.v)
		{
			temp = uv.u * u * width + uv.v * v * height;
			real += texture(sampler_0, vec2(u, v)) * cos(2 * PI * temp) - 
				texture(sampler_1, vec2(u, v)) * sin(2 * PI * temp);
		}
	}
	real = real / sqrt(width * height);
	if(real.r > 1.0f) real.r = 1.0f;
	if(real.r < 0.0f) real.r = 0.0f;
	if(real.g > 1.0f) real.g = 1.0f;
	if(real.g < 0.0f) real.g = 0.0f;
	if(real.b > 1.0f) real.b = 1.0f;
	if(real.b < 0.0f) real.b = 0.0f;
	if(real.a > 1.0f) real.a = 1.0f;
	if(real.a < 0.0f) real.a = 0.0f;
}

void main()
{
	float width = 1.0 / delta.u;
	float height = 1.0 / delta.v;
	if(!inverse)
	{
		vec4 re, im;
		dft(uv, width, height, re, im);
		atta_0 = re / sqrt(width * height);
		atta_1 = im / sqrt(width * height);
	}else{
		vec4 real;
		idft(uv, width, height, real);
		atta_0 = color;
	}
}