#version 460
precision highp float;

uniform sampler2D tex;

in vec2 texture_coords;
in vec4 color;

layout (location = 0) out vec4 frag_color;

void main ()
{
	vec4 t = texture (tex, texture_coords);
	frag_color = t * color;
}
