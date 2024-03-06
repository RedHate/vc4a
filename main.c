/**
 * 
 * "Voice chat 4 all"
 * 
 * 2024 redhate / ultros - https://github.con/redhate/vc4a
 * 
 * Data rate: 1024000 bytes / second
 * 
 * Bit Depth 	16bit (AUDIO_S16)
 * Rate 		4000 
 * Samples		128
 * Channels		1
 * 
 * @note the only way to exit is using the escape button!
 *
 */

#include <stdio.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_main.h>
#include <GL/gl.h>
#include <zlib.h>

#define FREQ 			4000
#define FORMAT 			AUDIO_S16 // (AUDIO_S16 0x0010) (AUDIO_S8 0x0008) // Not a datatype! is a definition
#define CHANNELS 		1
#define SAMPLES 		32
#define TYPE			short

// microphone vars
int mic_soft_gain  	=  1;

// network sockets
int a_socket 		= -1;

// buffer used for creating a matrices (needed a place to store the list)
TYPE out_waveform_buffer[CHANNELS*SAMPLES];
TYPE  in_waveform_buffer[CHANNELS*SAMPLES];

// input callback that updates mic input and the waveform buffer used to update vertex buffer objects
static void input_callback(void* userdata, Uint8* stream, int len){
	//softgain (preprocess the samples before they hit the buffer)
	int i;
	for(i=0;i<len/2;i++){
		short *ptr = (short*)stream;
		ptr[i] = (float)ptr[i] * mic_soft_gain;
	}
	// write tx stream to network socket if network socket is valid
	if(a_socket > 0){
		// write the stream to the socket
		if(write(a_socket, stream, len) > 0){
			// bytes were written no need to bitch about it
			memcpy(&out_waveform_buffer[0], &stream[0], len);
		} 
	}
}

// output callback that updates audio and the waveform buffer used to update vertex buffer objects
static void output_callback(void* userdata, Uint8* stream, int len){
	// write rx stream to playback stream
	if(a_socket > 0){
		// read any inbound socket data into the rx stream
		if(read(a_socket, stream, len) > 0){
			// bytes were read no need to bitch about it
			memcpy(&in_waveform_buffer[0], &stream[0], len);
		}
	}
}

SDL_AudioDeviceID init_audio(int direction, int format, int freq, int samples, int channels, int devicenum){

	// SDL audio specification
	SDL_AudioSpec AudioSpec;
	SDL_zero(AudioSpec);
	AudioSpec.format 	= format;
	AudioSpec.freq 		= freq;
	AudioSpec.samples 	= samples;
	AudioSpec.channels	= channels;	

	// is it a listening thread or a playback thread?
	// this function throws a warning in GCC, (there must be a better way of assigning the callback)
	if(direction == 1){
		AudioSpec.callback = input_callback; //microphone thread
	}
	else{ 
		AudioSpec.callback = output_callback; //playback thread
	}
	
	// finally assign the device id and open device
	SDL_AudioSpec gReceivedSpec; // should allow the stream to adapt
	SDL_AudioDeviceID AudioDeviceID = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(devicenum, SDL_TRUE), direction ? SDL_TRUE : SDL_FALSE, &AudioSpec, &gReceivedSpec, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
	if(AudioDeviceID == 0){
		// Report error
		printf("Failed to open recording device! SDL Error: %s", SDL_GetError());
		return -1;
	}
	
	return AudioDeviceID;
	
}

int main(int argc, const char **argv){

	// make 
	if(argc < 3){
		printf("Usage:\r\n %s server <port>\r\n %s client <ip> <port>\r\n", &argv[0][0], &argv[0][0]);
		return 0;
	}

	// type defs. (unused outside of main no reason for globalisation)
	// its C keep it in scope.. *SCOPE*
	// texture uv's used by vertex_tcnv
	typedef struct uv{
		float   u,
				v;
	}uv;
	// color rgba used by vertex_tcnv
	typedef struct rgba{
		float   r,
				g,
				b,
				a;
	}rgba;
	// vertices used by vertex_tcnv
	typedef struct vertex{
		float   x,
				y,
				z;
	}vertex;
	// the vertex buffer object definition (this is flexible and doesnt have to have all these elements, i left them in there because it's the most full setting for draw)
	typedef struct vertex_tcnv{
		uv          t; // texture
		rgba        c; // color
		vertex      n, // normals
					v; // vertex
	} vertex_tcnv;

	// bandwidth usage per-second
	int bw = (int)(FREQ*CHANNELS*SAMPLES*sizeof(TYPE));
	printf("Bandwidth: %d bytes (per second)\r\n", bw);

	// init sdl audio
	if(SDL_Init(SDL_INIT_AUDIO) < 0){
		printf("SDL could not initialize! SDL Error: %s\n", SDL_GetError());
		return 1;
	}

	// check for pressence of any audio device
	int AudioDeviceCount = SDL_GetNumAudioDevices(SDL_TRUE);
	if(AudioDeviceCount < 1){
		printf( "Unable to get audio capture device! SDL Error: %s\n", SDL_GetError() );
		return -1;
	}

	// list the audio devices for use
	int i;
	for(i=0; i<AudioDeviceCount; i++){
		// Get capture device name
		const char* deviceName = SDL_GetAudioDeviceName(i, SDL_TRUE);
		printf("%d - %s\n", i, deviceName);
	}

	// audio device selection
	int index;
	printf("Choose audio\n");
	int ret = scanf("%d", &index);
	if(ret < 0){
		printf("Input failure\r\n");
	}

	// init the input and output threads
	SDL_AudioDeviceID AudioInputDevice  = init_audio(1, FORMAT, FREQ, SAMPLES, CHANNELS, index);
	SDL_AudioDeviceID AudioOutputDevice = init_audio(0, FORMAT, FREQ, SAMPLES, CHANNELS, index);

	// network initialization function returns socket
	int start_network(const char *address, const char *port, int mode){
		
		// socket code 
		int a_socket = 0;
		int s_socket = 0;
		socklen_t clilen;
		struct sockaddr_in serv_addr, cli_addr;
		
		int sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd < 0){ 
			printf("ERROR opening socket\r\n");
			return -1;
		}
		
		// server mode
		if(mode){
			
			bzero((char *) &serv_addr, sizeof(serv_addr));
			serv_addr.sin_family = AF_INET;
			serv_addr.sin_addr.s_addr = INADDR_ANY;
			serv_addr.sin_port = htons(atoi(port));
			
			if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
				printf("ERROR on binding\r\n");
				return -1;
			}
			
			listen(sockfd,5);
			clilen = sizeof(cli_addr);
			
			s_socket = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
			if(s_socket < 0){ 
				printf("ERROR on accept\r\n");
				return -1;
			}
			
			a_socket = s_socket;
			
		}
		// client mode
		else{
			
			struct hostent *server = gethostbyname(address);
			if (server == NULL){
				printf("ERROR, no such host\n");
				return -1;
			}

			bzero((char *) &serv_addr, sizeof(serv_addr));
			serv_addr.sin_family = AF_INET;
			bcopy((char *)server->h_addr,(char *)&serv_addr.sin_addr.s_addr, server->h_length);
			serv_addr.sin_port = htons(atoi(port));

			if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0){
				printf("ERROR connecting\n");
				return -1;
			}
			
			a_socket = sockfd;
			
		}
		
		return a_socket;
		
	}

	// init network connection
	if(strcmp(&argv[1][0],"server")==0){
		// server
		printf("Starting in server mode, waiting peer for a connection...\r\n");
		if((a_socket = start_network("localhost", &argv[2][0], 1)) < 0){
			return 0;
		}
	}
	else{
		// client
		printf("Starting in client mode.\r\n");
		if((a_socket = start_network( &argv[2][0],  &argv[3][0], 0)) < 0){
			return 0;
		}
	}
	printf("Peer connection esablished.\r\n");

	// unpause the playback thread (start playing)
	SDL_PauseAudioDevice( AudioOutputDevice, SDL_FALSE);  

	// init sdl video
	SDL_Init( SDL_INIT_VIDEO );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY );
	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE,  16 );
	SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, 8 );

	// init an sdl window
	int WINDOW_WIDTH  = 480;
	int WINDOW_HEIGHT = 272;
	static SDL_Window* a_window = NULL;

	// local function for window resizing, doesnt need to be global
	void Resized(int w, int h){
		int dw, dh;
		WINDOW_WIDTH = w; WINDOW_HEIGHT = h;
		SDL_GL_GetDrawableSize(a_window, &dw, &dh);
	}

	// create a wiindow
	a_window = SDL_CreateWindow( "vc4a", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
							 WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE |  
							 /*SDL_WINDOW_INPUT_GRABBED |*/ SDL_WINDOW_OPENGL );

	// create an sdl gl context
	SDL_GLContext gl_ctx = SDL_GL_CreateContext(a_window);
	SDL_GL_MakeCurrent(a_window, gl_ctx);

	// set up open gl
	glLineWidth(2.0f);
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
 	glDepthFunc(GL_LEQUAL);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	// set up view matrix
	glDepthRange(0.0f, 1.0f);
	glPolygonOffset( WINDOW_WIDTH, WINDOW_HEIGHT);
	glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
	glScissor(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

	// local perspective function, vely important. doesnt need to be global
	void Perspective(float yfov, float aspect, float znear, float zfar){
		float xmin, xmax, ymin, ymax;
		ymax = znear * tan( yfov * M_PI / 360.0f );
		ymin = -ymax;
		xmin = ymin * aspect;
		xmax = ymax * aspect;
		glFrustum(xmin,xmax,ymin,ymax,znear,zfar);
	}

	void DrawVBO(vertex_tcnv *vbo){
		
		// model view
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		{
			// position and rotation of model is for the most part ignored
			glTranslatef(0, 0, -2.0f);
			glRotatef(0, 0, 0, 1);
			glRotatef(0, 0, 1, 0);
			glRotatef(0, 1, 0, 0);
		}

		// enable blending and material
		glEnable(GL_BLEND);
		glEnable(GL_COLOR_MATERIAL);
		
		// push the matrix
		glPushMatrix(); 

		// enable the elements we want
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);
		glEnableClientState(GL_NORMAL_ARRAY);
		glEnableClientState(GL_VERTEX_ARRAY);

		// load and draw
		glTexCoordPointer(2, GL_FLOAT, 12*sizeof(float), &vbo->t);
		glColorPointer(4, GL_FLOAT,  12*sizeof(float), &vbo->c);
		glNormalPointer(GL_FLOAT,  12*sizeof(float), &vbo->n);
		glVertexPointer(3, GL_FLOAT, 12*sizeof(float), &vbo->v);
		glDrawArrays(GL_LINE_STRIP, 0, SAMPLES);

		// disable the elements we wanted
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_NORMAL_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);
		
		// pop the matrix
		glPopMatrix();
		
		// disable materials and blending
		glDisable(GL_COLOR_MATERIAL);
		glDisable(GL_BLEND);
		glLoadIdentity();
		
	}

	// our two vertex buffer objects used to draw the waveforms
	vertex_tcnv in_waveform_vbo[SAMPLES];
	vertex_tcnv out_waveform_vbo[SAMPLES];

	//used for scaling the display
	float time_factor = 1.0f/(SAMPLES/5.0f); //figure this out programatically
	float amp_factor  = 0.0001f;  //figure this out programatically

	// main loop where we will add the control system that toggles the pauseaudiodevice function on / off
	int running = 1;
	while(running){
				
		SDL_Event ev;
		while(SDL_PollEvent(&ev)){
			// if its the same key skip it, skip it good.
			if ( ev.key.repeat ) break;
			// quit with window handle
			if(ev.type == SDL_QUIT){ 
				running = 0;
			}
			// resize the window
			if(ev.type == SDL_WINDOWEVENT){
				if( ev.window.event == SDL_WINDOWEVENT_CLOSE ){
					running = 0;
				}
			}
			// resize the window
			if(ev.type == SDL_WINDOWEVENT){
				if(( ev.window.event == SDL_WINDOWEVENT_RESIZED ) 	|| 
				   ( ev.window.event == SDL_WINDOWEVENT_MAXIMIZED ) || 
				   ( ev.window.event == SDL_WINDOWEVENT_RESTORED ) 	|| 
				   ( ev.window.event == SDL_WINDOWEVENT_MINIMIZED )){ 
					Resized( ev.window.data1, ev.window.data2 ); 
				}
			}
			// exit button
			if(ev.key.keysym.sym == SDLK_ESCAPE){
				if(ev.type == SDL_KEYDOWN){
					running = 0;
				}
			}
			// mic button (push to talk)
			if(ev.key.keysym.sym == SDLK_SPACE){
				if(ev.type == SDL_KEYDOWN){
					SDL_PauseAudioDevice(AudioInputDevice,  SDL_FALSE);
				}
				if(ev.type == SDL_KEYUP){
					SDL_PauseAudioDevice(AudioInputDevice,  SDL_TRUE);
				}
			}
			// latch button
			if(ev.key.keysym.sym == SDLK_l){
				if(ev.type == SDL_KEYDOWN){
					SDL_PauseAudioDevice(AudioInputDevice,  SDL_FALSE);
				}
			}
			
			//soft gain up
			if(ev.key.keysym.sym == SDLK_PLUS){
				if(ev.type == SDL_KEYDOWN){
					if(mic_soft_gain < 25.0f){
						mic_soft_gain += 1.0f;
					}
				}
			}
			//soft gain down
			if(ev.key.keysym.sym == SDLK_MINUS){
				if(ev.type == SDL_KEYDOWN){
					if(mic_soft_gain > 1.0f){
						mic_soft_gain -= 1.0f;
					}
				}
			}
			
			//amp
			if(ev.key.keysym.sym == SDLK_UP){
				if(ev.type == SDL_KEYDOWN){
					if(amp_factor < 0.1){
						amp_factor += 0.0001f;
					}
				}
			}

			if(ev.key.keysym.sym == SDLK_DOWN){
				if(ev.type == SDL_KEYDOWN){
					if(amp_factor > 0.0001){
						amp_factor -= 0.0001f;
					}
				}
			}


			//time
			if(ev.key.keysym.sym == SDLK_RIGHT){
				if(ev.type == SDL_KEYDOWN){
					if(time_factor < 0.1){
						time_factor += 0.001f;
					}
				}
			}

			if(ev.key.keysym.sym == SDLK_LEFT){
				if(ev.type == SDL_KEYDOWN){
					if(time_factor > 0.001){
						time_factor -= 0.001f;
					}
				}
			}
			
			
			
			
		}

		// update both vbos in a single pass 
		for (int i=0;i<SAMPLES;i++){
			 in_waveform_vbo[i] = (vertex_tcnv) { {1, 1}, {0,1,1,1}, {0, 0, 1}, {(float)((-SAMPLES/2)+i)*time_factor, (float)( in_waveform_buffer[i])*amp_factor, 0} };
			out_waveform_vbo[i] = (vertex_tcnv) { {1, 1}, {1,0,1,1}, {0, 0, 1}, {(float)((-SAMPLES/2)+i)*time_factor, (float)(out_waveform_buffer[i])*amp_factor, 0} };
		}

		// clear the screen with a nice dark gray
		glClearColor(0.025f, 0.025f, 0.025f, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		
		// set up the view matrix
		glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		{
			// rotation and position is not mutated in this project
			Perspective(75.0f, (float)WINDOW_WIDTH/WINDOW_HEIGHT, 0.1f, 1000.0f);
			glRotatef(0, 1, 0, 0);
			glRotatef(0, 0, 1, 0);
			glRotatef(0, 0, 0, 1);
			glTranslatef(0, 0, 0);
		}

		// draw the vertex buffer objects
		DrawVBO(in_waveform_vbo);
		DrawVBO(out_waveform_vbo);

		//dbgclear(NOCOLOR);
		//dbgprint(" Welcome!                             ", 1, 1,  0x88880000, WHITE, 4);
		//dbgprint("      Current Resolution is: %dx%d",   1, 2, WHITE, 0x88880000, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

		// flip gl
		SDL_GL_SwapWindow(a_window);
	}

	// terminate socket connection
	shutdown(a_socket, SHUT_RDWR);

	// pause input and playback
	SDL_PauseAudioDevice(AudioInputDevice,  SDL_TRUE);
	SDL_PauseAudioDevice(AudioOutputDevice, SDL_TRUE);

	// close audio
	SDL_CloseAudioDevice(AudioInputDevice);
	SDL_CloseAudioDevice(AudioOutputDevice);

	// close the window
	SDL_DestroyWindow(a_window);

	// exit sdl
	SDL_Quit(); 

	// exit smooth
	return 0;
	
}

