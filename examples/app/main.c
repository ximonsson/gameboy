/** -----------------------------------------------------------------------------------------------
 *  File: main.c
 *  Description: Test application to run the Game Boy emulator. Runs with OpenGL (ES).
 *  ----------------------------------------------------------------------------------------------- */
#include "gb.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pulse/simple.h>
#include <pulse/error.h>

#ifdef GLES
#include <GLES2/gl2.h>
#else
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#include <SDL2/SDL.h>

#define TITLE "GAME BOY"

#define VERTEX_SHADER_FILE "examples/app/shaders/vertex.glsl"
#define FRAGMENT_SHADER_FILE "examples/app/shaders/fragment.glsl"

static int width, height;
static SDL_Window *sdl_window;
static SDL_GLContext sdl_context;

static GLuint image_texture, program, vertexshader, fragmentshader, color_uniform, vertex_vbo, texture_vbo;

static GLfloat texture_coords[6 * 2] =
{
	0.0, 1.0,
	0.0, 0.0,
	1.0, 0.0,
	1.0, 0.0,
	1.0, 1.0,
	0.0, 1.0
};

static GLfloat vertex_coords[6 * 3] =
{
	-1.0, -1.0,  0.0,
	-1.0,  1.0,  0.0,
	1.0,  1.0,  0.0,
	1.0,  1.0,  0.0,
	1.0, -1.0,  0.0,
	-1.0, -1.0,  0.0
};


static int compile_shaders ()
{
	int result, file_size;
	char* fragment_shader, * vertex_shader;

	FILE* fp = fopen (VERTEX_SHADER_FILE, "rb");
	if (!fp)
	{
		fprintf (stderr, "could not open vertex shader file\n");
		return 1;
	}
	fseek (fp, 0, SEEK_END);
	file_size = ftell (fp);
	rewind (fp);
	vertex_shader = (char *) malloc (file_size);
	memset (vertex_shader, 0, file_size);

	if ((result = fread (vertex_shader, 1, file_size - 1, fp)) != file_size - 1)
	{
		fprintf (stderr, "could not read entire vertex shader\n");
		return 1;
	}
	fclose (fp);

	fp = fopen (FRAGMENT_SHADER_FILE, "rb");
	if (!fp)
	{
		fprintf (stderr, "could not open fragment shader file\n");
		return 1;
	}

	fseek (fp, 0, SEEK_END);
	file_size = ftell (fp);
	rewind (fp);
	fragment_shader = (char*) malloc (file_size);
	memset (fragment_shader, 0, file_size);

	if ((result = fread (fragment_shader, 1, file_size - 1, fp)) != file_size - 1)
	{
		fprintf (stderr, "could not read entire vertex shader\n");
		return 1;
	}
	fclose (fp);

	vertexshader 	= glCreateShader (GL_VERTEX_SHADER);
	fragmentshader 	= glCreateShader (GL_FRAGMENT_SHADER);

	glShaderSource (vertexshader,   1, (const char **) &vertex_shader,   0);
	glShaderSource (fragmentshader, 1, (const char **) &fragment_shader, 0);

	int status;
	glCompileShader (vertexshader);
	glGetShaderiv   (vertexshader, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE)
	{
		GLint log_size = 0;
		glGetShaderiv (vertexshader, GL_INFO_LOG_LENGTH, &log_size);

		char error_log[log_size];
		glGetShaderInfoLog (vertexshader, log_size, &log_size, error_log);

		fprintf (stderr, "error compiling vertex shader\n%s\n", error_log);
		return 1;
	}

	glCompileShader (fragmentshader);
	glGetShaderiv   (fragmentshader, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE)
	{
		GLint log_size = 0;
		glGetShaderiv (fragmentshader, GL_INFO_LOG_LENGTH, &log_size);

		char error_log[log_size];
		glGetShaderInfoLog (fragmentshader, log_size, &log_size, error_log);
		fprintf (stderr, "error compiling fragment shader\n%s\n", error_log);
		return 1;
	}

	// create the shader program and attach the shaders
	program = glCreateProgram ();

	glAttachShader (program, fragmentshader);
	glAttachShader (program, vertexshader);
	glLinkProgram  (program);
	glUseProgram   (program);

	free (vertex_shader);
	free (fragment_shader);
	return 0;
}


int init_screen (int w, int h)
{
	width = w;
	height = h;

	if (SDL_Init (SDL_INIT_VIDEO) < 0)
	{
		fprintf (stderr, "error init sdl video\n");
		return 1;
	}

	SDL_GL_SetAttribute (SDL_GL_DOUBLEBUFFER, 1);
#ifdef GLES
	SDL_GL_SetAttribute (SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#endif

	sdl_window  = SDL_CreateWindow (
		TITLE,
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		width,
		height,
		SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS
	);
	sdl_context = SDL_GL_CreateContext (sdl_window);
	SDL_GL_SetSwapInterval (1);
	return 0;
}


int init_opengl ()
{
	const GLubyte* renderer_name = glGetString (GL_RENDERER);
	printf (" > OpenGL renderer: %s\n", renderer_name);

	const GLubyte* opengl_version = glGetString (GL_VERSION);
	printf (" > OpenGL version: %s\n", opengl_version);

	const GLubyte* glsl_versions = glGetString (GL_SHADING_LANGUAGE_VERSION);
	printf (" > GLSL version: %s\n", glsl_versions);

	if (compile_shaders () != 0)
		return 1;

	glClearColor (.1f, .1f, .1f, 1.f);

	// gen texture
	glEnable 		(GL_TEXTURE_2D);
	glActiveTexture	(GL_TEXTURE0);
	glGenTextures 	(1, &image_texture);
	glBindTexture 	(GL_TEXTURE_2D, image_texture);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// generate vertex buffer for vertices and texture coords
	glGenBuffers	(1, &vertex_vbo);
	glGenBuffers 	(1, &texture_vbo);

	glBindBuffer (GL_ARRAY_BUFFER, vertex_vbo);
	glBufferData (GL_ARRAY_BUFFER, 6 * 3 * sizeof (GLfloat), vertex_coords, GL_STATIC_DRAW);
	GLuint vertex_attrib_location = glGetAttribLocation (program, "vertex");
	glEnableVertexAttribArray (vertex_attrib_location);
	glVertexAttribPointer (vertex_attrib_location, 3, GL_FLOAT, 0, 0, 0);

	glBindBuffer (GL_ARRAY_BUFFER, texture_vbo);
	glBufferData (GL_ARRAY_BUFFER, 6 * 2 * sizeof (GLfloat), texture_coords, GL_STATIC_DRAW);
	GLuint texture_attrib_location = glGetAttribLocation (program, "texture_coords_in");
	glEnableVertexAttribArray (texture_attrib_location);
	glVertexAttribPointer (texture_attrib_location, 2, GL_FLOAT, 0, 0, 0);

	glUniform1i (glGetUniformLocation (program, "tex"), 0);

	color_uniform = glGetAttribLocation (program, "color_in");
	glVertexAttrib4f (color_uniform, 1.0, 1.0, 1.0, 1.0);

	return 0;
}

void quit_opengl ()
{
	SDL_GL_DeleteContext (sdl_context);
	SDL_DestroyWindow (sdl_window);
	SDL_Quit ();
}

void draw ()
{
	// in case window has been resized get the new size
	SDL_GetWindowSize (sdl_window, &width, &height);
	glViewport (0, 0, width, height);

	const uint8_t* screen = gb_lcd ();
	glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, GB_LCD_WIDTH, GB_LCD_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, screen);

	glClear (GL_COLOR_BUFFER_BIT);
	glDrawArrays (GL_TRIANGLES, 0, 6);
	SDL_GL_SwapWindow (sdl_window);
}

// connection to Pulse Audio server
static pa_simple* audioconn;
static float* audio_samples_buffer;

// initialize audio connection
static void audio_init (int rate)
{
	pa_sample_spec ss;
	ss.format = PA_SAMPLE_FLOAT32NE;
	ss.channels = 2;
	ss.rate = rate;

	audio_samples_buffer = malloc (rate * sizeof (float));

	int error;
	audioconn = pa_simple_new (NULL, TITLE, PA_STREAM_PLAYBACK, NULL, TITLE, &ss, NULL, NULL, &error);
	if (!audioconn)
	{
		fprintf (stderr, "error pa_simple_new: %s\n", pa_strerror (error));
		exit (1);
	}
}

// play audio samples
static int audio_play ()
{
	static int err;
	static size_t size;

	gb_audio_samples (audio_samples_buffer, &size);

	if (pa_simple_write (audioconn, audio_samples_buffer, size, &err) < 0)
		fprintf (stderr, "pa_simple_write: %s\n", pa_strerror (err));

	return err;
}

// deinitialize audio
static void audio_quit ()
{
	int err;
	if (pa_simple_flush (audioconn, &err) < 0)
		fprintf (stderr, "pa_simple_flush: %s\n", pa_strerror (err));
	pa_simple_free (audioconn);
	free (audio_samples_buffer);
}

// if the game is running
static int running = 0;

// handle user input
static void handle_events ()
{
	SDL_Event event;
	void (*key_func) (gb_button);

	while (SDL_PollEvent (&event))
	{
		switch (event.type)
		{
			case SDL_QUIT:
				running = 0;
				break;

			case SDL_KEYDOWN:
			case SDL_KEYUP:
				key_func = event.type == SDL_KEYUP ? &gb_release_button : &gb_press_button;
				switch (event.key.keysym.sym)
				{
					case SDLK_a:
						key_func (gb_io_b_left);
						break;

					case SDLK_s:
						key_func (gb_io_b_down);
						break;

					case SDLK_d:
						key_func (gb_io_b_right);
						break;

					case SDLK_w:
						key_func (gb_io_b_up);
						break;

					case SDLK_j:
						key_func (gb_io_b_a);
						break;

					case SDLK_k:
						key_func (gb_io_b_b);
						break;

					case SDLK_SPACE:
						key_func (gb_io_b_start);
						break;

					case SDLK_x:
						key_func (gb_io_b_select);
						break;

					case SDLK_q:
						running = 0;
						break;
				}
			break;
		}
	}
}

#define SAMPLE_RATE 44100

int main (int argc, char** argv)
{
	gb_init (SAMPLE_RATE);

	printf ("\n");
	if (gb_load (argv[1]) != 0)
	{
		fprintf (stderr, "error opening game file\n");
		return 1;
	}
	printf ("\n");

	// init
	printf ("initializing sdl with opengl.\n");
	init_screen (GB_LCD_WIDTH * 4, GB_LCD_HEIGHT * 4);
	init_opengl ();

	printf ("initializing pulse audio output.\n");
	audio_init (SAMPLE_RATE);

	// run game
	printf ("starting game.\n");
	running = 1;
	while (running)
	{
		gb_step ();
		draw ();
		audio_play ();
		handle_events ();
	}

	// deinit
	gb_quit ();
	quit_opengl ();
	audio_quit ();
	return 0;
}
