#version 330
precision mediump float;

uniform sampler2D tex;
in vec2 texCoord;
out vec4 color;

void main( void ){
	color = texture2D( tex, texCoord );
}