//#define FREEGLUT_STATIC

//#include <windows.h>
//#include <stdio.h>

#include <string>
#include <iostream>
#include <fstream>
#include <array>
#include <vector>
#include <cmath>
#include <ctime>
#include <stdexcept>

#include "GL\glew.h"
#include "GL\freeglut.h"

#define GLM_FORCE_SWIZZLE
#include "glm\glm\glm.hpp"
#include "glm\glm\mat4x4.hpp"
#include "glm\glm\gtc\matrix_transform.hpp"

#define M_PI 3.14159265358979323846264338327950288

//
// Function declaration
//

// Initializitation
void setupRC();
void init();					
void createFramebuffer();			
void createSampler();
void createFullscreenQuad();
void createComputeProgram();
void createQuadProgram();
void createTexture();

// Loop
void loop();					// Main loop
void trace();					// Trace via compute shader
void present();					// Render via vertex/fragment shader
void save();					// Save current framebuffer

// Events
void onKeyPress( unsigned char, int, int );
void onWindowResize( int, int );
void APIENTRY onError( GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, const void* );

// Other
void save();					// TBD
void resizeTexture();			// Resizes the texture on change of window size


// Utility
std::string readShaderFile( const char* );
GLuint loadShader( const char*, GLenum );
static int nextPowerOfTwo( int );

/*
	Parameters
*/

// Image Size
int width = 1920;
int height = 1200;
// Perspective Projection
float fov = 45.0f * M_PI / 180.0f;
float aspect = static_cast<float>( width ) / static_cast<float>( height );
// Camera Rotation
float rotationY = 0.0f;
float zoom = 10.0f;
// Time
float rotationT = 4.0f;

/*
	GLuint
*/

GLuint frameBuffer = 0;
GLuint sampler = 0;
GLuint vao = 0;
GLuint vbo = 0;
GLuint cProgram = 0;
GLuint qProgram = 0;

GLuint texture = 0;

/*
	Fullscreen Quad Coordinates
*/
const GLfloat quadCords[] = {
	-1.0f, -1.0f, 0.0f,
	1.0f, -1.0f, 0.0f,
	1.0f, 1.0f, 0.0f,
	1.0f, 1.0f, 0.0f,
	-1.0f, 1.0f, 0.0f,
	-1.0f, -1.0f, 0.0f
};

/*
	Workgroup Sizes
*/
int workGroupSizeX;
int workGroupSizeY;

/*
	Uniforms
*/
int eyeUniform;
int ray00Uniform, ray10Uniform, ray01Uniform, ray11Uniform;
int uTimeUniform;

/*
	Other
*/
int framebufferBinding;
int textureBinding;
bool resetFramebuffer = true;

/*
	Matrices
*/
glm::mat4 projMatrix;
glm::mat4 viewMatrix;
glm::mat4 invViewProjMatrix;

/*
	Camera
*/
glm::vec4 cameraPosition = glm::vec4( 5.0f, 10.0f, -5.0f, 1.0f );
glm::vec4 cameraLookAt = glm::vec4( 0.0f, 0.0f, 0.0f, 1.0f );
glm::vec4 cameraUp = glm::vec4( 0.0f, 1.0f, 0.0f, 1.0f );

/*
	Textures
*/
std::vector<char> textureData;

int main( int argc, char** argv ){

	glutInit( &argc, argv );
	glutInitDisplayMode( GLUT_SINGLE | GLUT_RGBA );

	glutInitWindowSize( width, height );
	glutInitWindowPosition( 100, 100 );
	glutCreateWindow( "RayTracer" ); 
	
	GLenum err = glewInit();
	if( err != GLEW_OK ){
		std::cout << "Error when initializing glew..." << std::endl;
		return -1;
	}

	// Enable Error Messages
	glEnable( GL_DEBUG_OUTPUT );
	glDebugMessageCallback( onError, 0 );
	// */

	setupRC();
	init();

	glutDisplayFunc( loop );
	glutReshapeFunc( onWindowResize );
	glutKeyboardFunc( onKeyPress );
	glutMainLoop();
}

void setupRC(){
	glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
}

void init(){
	// load checkerboard texture
	 createTexture();

	// create texture/framebuffer
	createFramebuffer();

	// create sampler
	createSampler();

	// create VAO & VBO for rendering
	createFullscreenQuad();

	// load compute shader
	createComputeProgram();

	// load quad shader
	createQuadProgram();
}

void loop(){

	glViewport( 0, 0, width, height );

	trace();

	present();
}

void trace(){
	glm::vec4 temp = glm::vec4();

	glUseProgram( cProgram );

	// Change camera position
	cameraPosition.x = static_cast<float>( sin( -rotationY ) * zoom );
	cameraPosition.y = 5.0f;
	cameraPosition.z = - static_cast<float>( cos( -rotationY ) * zoom );

	viewMatrix = glm::lookAt( cameraPosition.xyz(), cameraLookAt.xyz(), cameraUp.xyz() );

	if( resetFramebuffer ) {
		projMatrix = glm::perspective<float>( fov, aspect, 1.0f, 2.0f );
		resizeTexture();
		resetFramebuffer = false;
	}

	// Calculate inverse projection/view matrix
	invViewProjMatrix = glm::inverse( projMatrix * viewMatrix );

	// Set Uniforms according to that
	glUniform3f( eyeUniform, cameraPosition.x, cameraPosition.y, cameraPosition.z );
	temp = ( invViewProjMatrix * glm::vec4( -1.0f, -1.0f, 0.0f, 1.0f ) );
	temp = glm::vec4( temp.x / temp.w, temp.y / temp.w, temp.z / temp.w, 1.0f ) - cameraPosition;		//	Perspective Divide
	glUniform3f( ray00Uniform, temp.x, temp.y, temp.z );
	temp = ( invViewProjMatrix * glm::vec4( -1.0f, 1.0f, 0.0f, 1.0f ) );
	temp = glm::vec4( temp.x / temp.w, temp.y / temp.w, temp.z / temp.w, 1.0f ) - cameraPosition;
	glUniform3f( ray01Uniform, temp.x, temp.y, temp.z );
	temp = ( invViewProjMatrix * glm::vec4( 1.0f, -1.0f, 0.0f, 1.0f ) );
	temp = glm::vec4( temp.x / temp.w, temp.y / temp.w, temp.z / temp.w, 1.0f ) - cameraPosition;
	glUniform3f( ray10Uniform, temp.x, temp.y, temp.z );
	temp = ( invViewProjMatrix * glm::vec4( 1.0f, 1.0f, 0.0f, 1.0f ) );
	temp = glm::vec4( temp.x / temp.w, temp.y / temp.w, temp.z / temp.w, 1.0f ) - cameraPosition;
	glUniform3f( ray11Uniform, temp.x, temp.y, temp.z );

	// Bind framebuffer texture as writable in shader
	glBindImageTexture( framebufferBinding, frameBuffer, 0, false, 0, GL_WRITE_ONLY, GL_RGBA32F );
	// Bind texture of floor as readable in shader
	glBindImageTexture( textureBinding, texture, 0, false, 0, GL_READ_ONLY, GL_RGBA32F );
	// Send time
	glUniform1f( uTimeUniform, rotationT );

	// Compute workgroup sizes
	int workSizeX = nextPowerOfTwo( width );
	int workSizeY = nextPowerOfTwo( height );

	// Invoke compute shader
	glDispatchCompute( workSizeX / workGroupSizeX, workSizeY / workGroupSizeY, 1 );

	// Synchronize framebuffer writes
	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

	// Reset
	glBindImageTexture( framebufferBinding, 0, 0, false, 0, GL_WRITE_ONLY, GL_RGBA32F );
	glBindImageTexture( textureBinding, 0, 0, false, 0, GL_READ_ONLY, GL_RGBA32F );
	glUseProgram( 0 );
}

void present(){

	glClear( GL_COLOR_BUFFER_BIT );

	// Bind texture
	glUseProgram( qProgram );
	glBindVertexArray( vao );
	glBindTexture( GL_TEXTURE_2D, frameBuffer );
	glBindSampler( frameBuffer, sampler );
	glDrawArrays( GL_TRIANGLES, 0, 6 );

	// Reset
	glBindSampler( 0, 0 );
	glBindTexture( GL_TEXTURE_2D, 0 );
	glBindVertexArray( 0 );
	glUseProgram( 0 );

	glFlush();
}

void save(){
	int size = width * height * 3;

	std::vector<GLfloat> pixels;
	pixels.resize( size );

	glReadPixels( 0, 0, width, height, GL_RGB, GL_FLOAT, &pixels[ 0 ] );

	std::ofstream fileStream;
	fileStream.open( "image.ppm" );

	// Write header
	fileStream << "P3\n" << width << " " << height << "\n255\n";

	for( int i = size - 3 ; i >= 0 ; i -= 3 ){

		GLfloat r = pixels[ i ] * 255;
		GLfloat g = pixels[ i + 1 ] * 255;
		GLfloat b = pixels[ i + 2 ] * 255;

		fileStream << r << " " << g << " " << b << "\t";

		if( i % width == width - 1 )
			fileStream << "\n";
	}

	fileStream.close();
}

void onKeyPress( unsigned char key, int x, int y ){
	switch( key ){
		case 'w':
			zoom -= 0.1f;
			break;
		case 'a':				// left
			rotationY += 0.04f;
			break;
		case 's':
			zoom += 0.1f;
			break;
		case 'd':				// right
			rotationY -= 0.04f;
			break;
		case 't':
			rotationT -= 0.04f;
			break;
		case 'y':
			rotationT += 0.04f;
			break;
		case 'p':
			save();
			break;
		case 'r':
			
			break;
	}

	glutPostRedisplay();
}

void onWindowResize( int w, int h ){
	width = w;
	height = h;
	resetFramebuffer = true;
}

void APIENTRY onError( GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam ){
	std::cout << "GL CALLBACK: " << ( type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR ** " : "" ) << "type = 0x" << type << ", severity = 0x" << severity << ", message = " << message << std::endl;
}

void createFramebuffer(){
	std::cout << "Creating framebuffer..." << std::endl;

	glGenTextures( 1, &frameBuffer );
	glBindTexture( GL_TEXTURE_2D, frameBuffer );
	glTexStorage2D( GL_TEXTURE_2D, 1, GL_RGBA32F, width, height );
	//glBindTexture( GL_TEXTURE_2D, 0 );

	std::cout << "Done.\n" << std::endl;
}

void createSampler(){
	std::cout << "Creating Sampler..." << std::endl;

	glGenSamplers( 1, &sampler );
	glSamplerParameteri( sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glSamplerParameteri( sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST );

	std::cout << "Done.\n" << std::endl;
}

void createFullscreenQuad(){
	std::cout << "Creating Quad..." << std::endl;

	glGenVertexArrays( 1, &vao );
	glBindVertexArray( vao );
	glGenBuffers( 1, &vbo );
	glBindBuffer( GL_ARRAY_BUFFER, vbo );
	glBufferData( GL_ARRAY_BUFFER, sizeof( quadCords ), quadCords, GL_STATIC_DRAW );
	glEnableVertexAttribArray( 0 );
	glVertexAttribPointer( 0, 3, GL_FLOAT, false, 0, 0L );
	glBindBuffer( GL_ARRAY_BUFFER, 0 );
	glBindVertexArray( 0 );

	std::cout << "Done.\n" << std::endl;
}

void createComputeProgram(){
	std::cout << "Creating Compute Program..." << std::endl;

	int workGroupSize[ 3 ];
	int parameters[ 1 ];

	cProgram = glCreateProgram();
	GLuint computeShader = loadShader( "GLSL/raytrace.comp", GL_COMPUTE_SHADER );
	glAttachShader( cProgram, computeShader );
	glLinkProgram( cProgram );
	glUseProgram( cProgram );
	glGetProgramiv( cProgram, GL_COMPUTE_WORK_GROUP_SIZE, workGroupSize );
	workGroupSizeX = workGroupSize[ 0 ];
	workGroupSizeY = workGroupSize[ 1 ];
	eyeUniform = glGetUniformLocation( cProgram, "eye" );
	ray00Uniform = glGetUniformLocation( cProgram, "ray00" );
	ray10Uniform = glGetUniformLocation( cProgram, "ray10" );
	ray01Uniform = glGetUniformLocation( cProgram, "ray01" );
	ray11Uniform = glGetUniformLocation( cProgram, "ray11" );
	uTimeUniform = glGetUniformLocation( cProgram, "u_time" );
	// framebuffer
	int loc = glGetUniformLocation( cProgram, "frameBuffer" );
	glGetUniformiv( cProgram, loc, parameters );
	framebufferBinding = parameters[ 0 ];
	// texture
	loc = glGetUniformLocation( cProgram, "texture" );
	glGetUniformiv( cProgram, loc, parameters );
	textureBinding = parameters[ 0 ];
	glUseProgram( 0 );

	std::cout << "Done.\n" << std::endl;
}

void createQuadProgram(){
	std::cout << "Creating Quad Program..." << std::endl; 

	qProgram = glCreateProgram();
	GLuint vertexShader = loadShader( "GLSL/quad.vert", GL_VERTEX_SHADER );
	GLuint fragmentShader = loadShader( "GLSL/quad.frag", GL_FRAGMENT_SHADER );
	glAttachShader( qProgram, vertexShader );
	glAttachShader( qProgram, fragmentShader );
	glBindAttribLocation( qProgram, 0, "vertex" );
	glBindFragDataLocation( qProgram, 0, "color" );
	glLinkProgram( qProgram );

	glUseProgram( qProgram );
	int texUniform = glGetUniformLocation( qProgram, "tex" );
	glUniform1i( texUniform, 0 );
	glUseProgram( 0 );

	std::cout << "Done.\n" << std::endl;
}

void createTexture( ){

	std::cout << "Creating texture..." << std::endl;

	const char* path = "textures/checkerboard.bmp";

	unsigned char header[ 54 ];
	unsigned int dataPos;
	unsigned int width, height;
	unsigned int size;

	unsigned char* data;

	FILE* file = fopen( path, "rb" );
	fread( header, 1, 54, file );

	dataPos = *( int* ) &( header[ 0x0A ] );
	size = *( int* ) &( header[ 0x22 ] );
	width = *( int* ) &( header[ 0x12 ] );
	height = *( int* ) &( header[ 0x16 ] );

	if( size == 0 ) size = width * height * 3;
	if( dataPos == 0 ) dataPos = 54;

	data = new unsigned char[ size ];
	fread( data, 1, size, file );
	fclose( file );

	glGenTextures( 1, &texture );
	glBindTexture( GL_TEXTURE_2D, texture );
	//glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, data );
	glTexStorage2D( GL_TEXTURE_2D, 1, GL_RGBA32F, width, height );
	glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, data );

	std::cout << "Done.\n" << std::endl;
}

void resizeTexture(){
	glDeleteTextures( 1, &frameBuffer );
	createFramebuffer();
}

std::string readShaderFile( const char* filename ){

	std::string content;
	std::ifstream fileStream( filename, std::ios::in );

	if( !fileStream.is_open() ){
		throw std::exception( "File does not exist." );
	}

	std::string line;
	while( !fileStream.eof() ){

		std::getline( fileStream, line );
		content.append( line + '\n' );
	}

	fileStream.close();

	return content;
}

GLuint loadShader( const char* path, GLenum shaderType ){
	
	std::cout << "Loading shader: " << path << std::endl;

	GLuint shader = glCreateShader( shaderType );

	std::string shaderString = readShaderFile( path );
	const char* shaderSource = shaderString.c_str();

	GLint result = GL_FALSE;
	int logLength;

	std::cout << "Compiling shader: " << path << std::endl;
	glShaderSource( shader, 1, &shaderSource, NULL );
	glCompileShader( shader );

	int rvalue;
	glGetShaderiv( shader, GL_COMPILE_STATUS, &rvalue );
	if( !rvalue ){
		std::cout << "Error when compiling shader..." << std::endl;
	}

	///*
	// nvoglv32.dll Error

	std::cout << "Checking shader: " << path << std::endl;
	glGetShaderiv( shader, GL_COMPILE_STATUS, &result );
	glGetShaderiv( shader, GL_INFO_LOG_LENGTH, &logLength );
	std::vector<GLchar> error( ( logLength >1 ) ? logLength : 1 );
	glGetShaderInfoLog( shader, logLength, NULL, &error[ 0 ] );
	std::cout << &error[ 0 ] << std::endl;
	// */

	std::cout << "Done." << std::endl;

	return shader;
}



/*
	Helper functions
*/
static int nextPowerOfTwo( int x ){
	x--;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x++;
	return x;

}

