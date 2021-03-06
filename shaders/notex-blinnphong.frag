#version 330

const uint LIGHT_SPOT = 0U;
const uint LIGHT_POINT = 1U;
const uint LIGHT_DIR = 2U;

struct light_s
{
	vec3 pos;
	uint type;

	vec3 dir;	
	float in_cutoff;

	vec3 color;
	float out_cutoff;
	
	float linear;
	float quadratic;
};

in vec3 f_normal;
in vec3 f_pos;
in vec4 f_light_pos;

out vec4 o_color;

uniform sampler2DShadow shadowmap;

uniform vec4 color;
uniform vec3 emission;
uniform float specular;

layout(std140) uniform ubodata
{
	vec3 ambient;
	float fog_density;

	vec3 fog_color;
	uint lights_count;

	light_s lights[8];
};

float calc_shadow()
{
	vec3 coords = f_light_pos.xyz;
	float bias = clamp(0.0025*tan(acos(clamp(dot(f_normal, -lights[0].dir), 0.0, 1.0))), 0, 0.01);

	//sampler PCF
	//float shadow = texture(shadowmap, vec3(coords.xy, coords.z - bias));

	//sampler PCF + PCF
	float shadow = 0.0;
	vec2 texel = 1.0 / textureSize(shadowmap, 0);
	for (float y = -1.5; y <= 1.5; y += 1.0)
		for (float x = -1.5; x <= 1.5; x += 1.0)
			shadow += texture(shadowmap, coords.xyz + vec3(vec2(x, y) * texel, -bias));
	shadow /= 16.0;
	
	return shadow;
}

vec3 apply_fog(vec3 color)
{
	float sun_amount = 0.0;
	if (lights_count >= 1U && lights[0].type == LIGHT_DIR)
		sun_amount = max(dot(normalize(f_pos), normalize(-lights[0].dir)), 0.0);
	vec3 fog_color_v = mix(fog_color, lights[0].color, pow(sun_amount, 30.0));
	float fog_amount_v = 1.0 - min(1.0, exp(-length(f_pos) * fog_density));
	return mix(color, fog_color_v, fog_amount_v);
}

float calc_light(vec3 light_dir)
{
	vec3 normal = normalize(f_normal);
	vec3 view_dir = normalize(vec3(0.0f, 0.0f, 0.0f) - f_pos);
	vec3 halfway_dir = normalize(light_dir + view_dir);
	
	float diffuse_v = max(dot(normal, light_dir), 0.0);
	float specular_v = pow(max(dot(normal, halfway_dir), 0.0), 15.0) * specular;
	
	return specular_v + diffuse_v;
}

float calc_point_light(light_s light)
{
	vec3 light_dir = normalize(light.pos - f_pos);
	float val = calc_light(light_dir);
	
	float distance = length(light.pos - f_pos);
	float atten = 1.0f / (1.0f + light.linear * distance + light.quadratic * (distance * distance));
	
	return val * atten;
}

float calc_spot_light(light_s light)
{
	vec3 light_dir = normalize(light.pos - f_pos);
	
	float theta = dot(light_dir, normalize(-light.dir));
	float epsilon = light.in_cutoff - light.out_cutoff;
	float intensity = clamp((theta - light.out_cutoff) / epsilon, 0.0, 1.0);

	float point = calc_point_light(light);	
	return point * intensity;
}

float calc_dir_light(light_s light)
{
	vec3 light_dir = normalize(-light.dir);
	return calc_light(light_dir);
}

void main()
{
	float shadow = calc_shadow();
	vec3 result = ambient * 0.3 + emission;
	for (uint i = 0U; i < lights_count; i++)
	{
		light_s light = lights[i];
		float part = 0.0;
		
		if (light.type == LIGHT_SPOT)
			part = calc_spot_light(light);
		else if (light.type == LIGHT_POINT)
			part = calc_point_light(light);
		else if (light.type == LIGHT_DIR)
			part = calc_dir_light(light);

		if (i == 0U)
			part *= shadow;
		result += light.color * part;
	}
	
	o_color = vec4(apply_fog(result * color.xyz), color.w);
}
