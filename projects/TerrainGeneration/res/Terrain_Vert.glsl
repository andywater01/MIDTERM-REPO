#version 410
layout(location = 0) in vec3 vertex_pos;
layout(location = 1) in vec3 vertex_color;
layout(location = 2) in vec2 vertex_uv;

out vec3 color;
out vec2 texUV;

uniform mat4 MVP;

uniform sampler2D myTextureSampler;

// Lecture 10b
uniform float time;

void main() {
	
	color = vertex_color;
	texUV = vertex_uv;

	//Lecture 10
	vec3 vert = vertex_pos;

	vert.y = texture(myTextureSampler, vertex_uv).r;
	//vert.x = texture(myTextureSampler, vertex_uv).g;
	//vert.z = texture(myTextureSampler, vertex_uv).b;
	/*if (vert.y >=0.9)
	{
		vert.y = vertex_pos.y;
		vert.y = 0.6;
	}
	else if (vert.y <= 0.1)
	{
		vert.y = vertex_pos.y;
		vert.y = 0.4;
	}
	else
	{
	vert.y = vertex_pos.y;
		vert.y = 0.5;
		//vert.y = mix(vert.y, vertex_pos.y, 0.6);
	}
	
	
		vec3 vert = vertex_pos;
	vert.x = texture(myTextureSampler, vertex_uv).r;
	vert.y = texture(myTextureSampler, vertex_uv).g;
	vert.z = texture(myTextureSampler, vertex_uv).b;
	
	if (vert.x > 0.9)
	{
		vert.x = vertex_pos.y;
		vert.x = 0.9;
	}

	
	else if (vert.x < 0.9)
	{
		vert.y = vertex_pos.y;
		vert.y = 0.9;
	}*/

	//vert.y = sin(vert.x*5.0 + time * 0.1)* 0.2;

	gl_Position = MVP * vec4(vert, 1.0);
		

	//gl_Position = MVP * vec4(vertex_pos, 1.0);
}
	