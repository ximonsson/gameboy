/** -----------------------------------------------------------------------------------------------
 *  File: main.c
 *  Description: Test application to run the Game Boy emulator. Runs with OpenGL (ES).
 *  ----------------------------------------------------------------------------------------------- */
#include "gb.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#ifndef AUDIO_SDL
#include <pulse/simple.h>
#include <pulse/error.h>
#endif

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

/**
 * helper function to read file contents to `data`.
 */
static int read_file (const char *fp, void **data, size_t *bytes)
{
	FILE* f = fopen (fp, "rb");
	if (!f)
	{
		fprintf (stderr, "could not open file @ %s\n", fp);
		return 1;
	}
	fseek (f, 0, SEEK_END);
	*bytes = ftell (f);
	rewind (f);
	*data = calloc (*bytes, 1);

	size_t ret = fread (*data, 1, *bytes, f);
	if (ret != *bytes)
	{
		fprintf (stderr, "could not read the entire file! got %ld of %ld bytes\n", ret, *bytes);
		return 1;
	}

	fclose (f);
	return 0;
}

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
	size_t file_size;
	char *fragment_shader, *vertex_shader;

	if (read_file (VERTEX_SHADER_FILE, (void **) &vertex_shader, &file_size) != 0)
	{
		fprintf (stderr, "failed to read vertex shader\n");
		return 1;
	}

	if (read_file (FRAGMENT_SHADER_FILE, (void **) &fragment_shader, &file_size) != 0)
	{
		fprintf (stderr, "failed to read fragment shader\n");
		return 1;
	}

	vertexshader = glCreateShader (GL_VERTEX_SHADER);
	fragmentshader = glCreateShader (GL_FRAGMENT_SHADER);

	glShaderSource (vertexshader, 1, (const char **) &vertex_shader, 0);
	glShaderSource (fragmentshader, 1, (const char **) &fragment_shader, 0);

	int status;
	glCompileShader (vertexshader);
	glGetShaderiv (vertexshader, GL_COMPILE_STATUS, &status);

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
	glGetShaderiv (fragmentshader, GL_COMPILE_STATUS, &status);

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
	glLinkProgram (program);
	glUseProgram (program);

	free (vertex_shader);
	free (fragment_shader);
	return 0;
}


int init_screen (int w, int h)
{
	width = w;
	height = h;

	if (SDL_Init (SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
	{
		fprintf (stderr, "error init sdl video\n");
		return 1;
	}

	SDL_GL_SetAttribute (SDL_GL_DOUBLEBUFFER, 1);
#ifdef GLES
	SDL_GL_SetAttribute (SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#endif

	sdl_window = SDL_CreateWindow (
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

#ifdef AUDIO_SDL

static float *audio_samples_buffer;
static SDL_AudioDeviceID audio_devid;

#else

// connection to Pulse Audio server
static pa_simple* audioconn;
static float* audio_samples_buffer;

#endif

// initialize audio connection
static void audio_init (int rate)
{
#ifdef AUDIO_SDL
	printf ("SDL_GetAudioDeviceName = %s\n", SDL_GetAudioDeviceName (0, 0));

	SDL_AudioSpec wanted = { 0 };
	wanted.freq = rate;
	wanted.format = AUDIO_F32SYS;
	wanted.channels = 2;
	wanted.samples = 1024;  // 1024
	//wanted.callback = NULL;
	//wanted.userdata = NULL;

	audio_devid = SDL_OpenAudioDevice (NULL, 0, &wanted, NULL, 0);

	if (audio_devid == 0)
	{
		fprintf (stderr, "error opening audio: %s\n", SDL_GetError ());
		exit (1);
	}

	audio_samples_buffer = malloc (rate * sizeof (float));

	SDL_PauseAudioDevice (audio_devid, 0);
#else
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
#endif
}

// play audio samples
static int audio_play ()
{
#ifdef AUDIO_SDL
	int ret = 0;
	size_t size;

	gb_audio_samples (audio_samples_buffer, &size);
	if ((ret = SDL_QueueAudio (audio_devid, audio_samples_buffer, size)) != 0)
	{
		fprintf (stderr, "[error] audio_play > could not queue samples.\n");
		return ret;
	}

	// halt emulation while we are playing the audio

	Uint32 q = SDL_GetQueuedAudioSize (audio_devid);
	while (q >= 1024 * 2 * sizeof (float)) // while more than 1024 samples are queued we pause emulation
	{
		usleep (200);
		q = SDL_GetQueuedAudioSize (audio_devid);
	}

	return 0;
#else
	int err;
	size_t size;

	gb_audio_samples (audio_samples_buffer, &size);

	if (pa_simple_write (audioconn, audio_samples_buffer, size, &err) < 0)
		fprintf (stderr, "pa_simple_write: %s\n", pa_strerror (err));

	return err;
#endif
}

// deinitialize audio
static int audio_quit ()
{
#ifdef AUDIO_SDL
	SDL_CloseAudioDevice (audio_devid);
	free (audio_samples_buffer);
	return 0;
#else
	int err;
	size_t size;

	gb_audio_samples (audio_samples_buffer, &size);

	if (pa_simple_write (audioconn, audio_samples_buffer, size, &err) < 0)
		fprintf (stderr, "pa_simple_write: %s\n", pa_strerror (err));

	return err;
#endif
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
#define SAMPLE_BUFFER 1024

int main (int argc, char** argv)
{
	// init

	size_t bytes;

	// emulator and load ROM
	gb_init (SAMPLE_RATE);

	// ROM data
	uint8_t *rom;
	if (read_file (argv[1], (void **) &rom, &bytes) != 0)
	{
		fprintf (stderr, "error reading game data\n");
		exit (1);
	}
	// third argument is location of the battery backed ram data.
	uint8_t *ram = NULL;
	if (argc == 3)
	{
		if (read_file (argv[2], (void **) &ram, &bytes) != 0)
		{
			fprintf (stderr, "could not load battery backed RAM @ %s\n", argv[2]);
			fprintf (stderr, "will continue like nada\n");
		}
	}

	// load the game
	if (gb_load (rom, ram) != 0)
		exit (1);

	// video
	printf ("initializing video.\n");
	init_screen (GB_LCD_WIDTH * 4, GB_LCD_HEIGHT * 4);
	init_opengl ();

	// audio
	printf ("initializing audio device.\n");
	audio_init (SAMPLE_RATE);

	// run

	printf ("starting game.\n");
	// the number of cpu cycles to run to get the wanted number of audio
	// samples to buffer.
	const uint32_t STEP = GB_CPU_CLOCK / SAMPLE_RATE * SAMPLE_BUFFER;
	uint32_t cc = 0;  // keep track of the CPU cycles.
	running = 1;

	while (running)
	{
		cc += gb_step (STEP);

		if (cc >= GB_FRAME)
		{
			// new frame
			draw ();
			cc -= GB_FRAME;
		}
		audio_play ();
		handle_events ();
	}

	// deinit
	gb_quit ();
	quit_opengl ();
	audio_quit ();

	free (rom);

	// store RAM for next run.
	if (argc == 3)
	{
		FILE *f = fopen (argv[2], "wb");
		// TODO communicate size better
		fwrite (ram, 1, 0x2000 * 4, f);
	}

	free (ram);

	return 0;
}
