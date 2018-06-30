#version 330
precision mediump float;

in vec2 vertex;
out vec2 texCoord;

void main( void ) {
	gl_Position = vec4( vertex, 0.0, 1.0 );
	texCoord = vertex * 0.5 + vec2( 0.5, 0.5 );		// Intervall Anpassung
}