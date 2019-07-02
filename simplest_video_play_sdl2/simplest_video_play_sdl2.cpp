/**
 * ��򵥵�SDL2������Ƶ�����ӣ�SDL2����RGB/YUV��
 * Simplest Video Play SDL2 (SDL2 play RGB/YUV) 
 *
 * ������ Lei Xiaohua
 * leixiaohua1020@126.com
 * �й���ý��ѧ/���ֵ��Ӽ���
 * Communication University of China / Digital TV Technology
 * http://blog.csdn.net/leixiaohua1020
 *
 * ������ʹ��SDL2����RGB/YUV��Ƶ�������ݡ�SDLʵ�����ǶԵײ��ͼ
 * API��Direct3D��OpenGL���ķ�װ��ʹ���������Լ���ֱ�ӵ��õײ�
 * API��
 *
 * �������ò�������: 
 *
 * [��ʼ��]
 * SDL_Init(): ��ʼ��SDL��
 * SDL_CreateWindow(): �������ڣ�Window����
 * SDL_CreateRenderer(): ���ڴ��ڴ�����Ⱦ����Render����
 * SDL_CreateTexture(): ��������Texture����
 *
 * [ѭ����Ⱦ����]
 * SDL_UpdateTexture(): ������������ݡ�
 * SDL_RenderCopy(): �����Ƹ���Ⱦ����
 * SDL_RenderPresent(): ��ʾ��
 *
 * This software plays RGB/YUV raw video data using SDL2.
 * SDL is a wrapper of low-level API (Direct3D, OpenGL).
 * Use SDL is much easier than directly call these low-level API.  
 *
 * The process is shown as follows:
 *
 * [Init]
 * SDL_Init(): Init SDL.
 * SDL_CreateWindow(): Create a Window.
 * SDL_CreateRenderer(): Create a Render.
 * SDL_CreateTexture(): Create a Texture.
 *
 * [Loop to Render data]
 * SDL_UpdateTexture(): Set Texture's data.
 * SDL_RenderCopy(): Copy Texture to Render.
 * SDL_RenderPresent(): Show.
 */

#include <stdio.h>

extern "C"
{
#include "sdl/SDL.h"
};

///////////////////////////////////////////////////////
/*
2019/6/13 ���޸�

����ڇLԇ���g�r�������F�����e�`��
1>SDL2main.lib(SDL_windows_main.obj) : error LNK2019: unresolved external symbol __imp__fprintf referenced in function _ShowError
1>SDL2main.lib(SDL_windows_main.obj) : error LNK2019: unresolved external symbol __imp____iob_func referenced in function _ShowError
1>E:DocumentsVisual Studio 2015ProjectsSDL2_TestDebugSDL2_Test.exe : fatal error LNK1120: 2 unresolved externals
����ԭ��
�� Visual Studio 2015�У��˜ʘ˜ʣ�stderr��stdout���x���£�

#define stdin (__acrt_iob_func(0))
#define stdout (__acrt_iob_func(1))
#define stderr (__acrt_iob_func(2))
������ǰ�����������x�飺

#define stdin (&__iob_func()[0])
#define stdout (&__iob_func()[1])
#define stderr (&__iob_func()[2])
���@�e�F��__iob_func���ٶ��x�ˣ��@������ʹ���c��ǰ�汾��Visual Studio �汾���g��. lib �ļ���朽��e�`��

���˽�Q�@�����}������ԇLԇ���x __iob_func()������ԓ����һ������ {*stdin,*stdout,*stderr}��array ��

�P�stdio����( ���ҿ����� sprintf() )������朽��e�`������Ԍ� legacy_stdio_definitions.lib ��ӵ�朽����x헡�

����ͨ�^VS2015���gSDL2Դ���a��Q�ô���
*/
#if 1
FILE _iob[] = { *stdin, *stdout, *stderr };
extern"C" FILE * __cdecl __iob_func(void)
{
	return _iob;
}
#endif
/////////////////////////////////////////////////////////
#define F_480_272
const int bpp=12;

int screen_w=500,screen_h=500;
#ifdef F_480_272
	const int pixel_w = 480, pixel_h = 272;
	//const int pixel_w = 1280, pixel_h = 720;
#else
	const int pixel_w = 320, pixel_h = 180;
	//const int pixel_w = 640, pixel_h = 272;
#endif


unsigned char buffer[pixel_w*pixel_h*bpp/8];


//Refresh Event
#define REFRESH_EVENT  (SDL_USEREVENT + 1)

#define BREAK_EVENT  (SDL_USEREVENT + 2)

int thread_exit=0;

int refresh_video(void *opaque){
	thread_exit=0;
	while (!thread_exit) {
		SDL_Event event;
		event.type = REFRESH_EVENT;
		SDL_PushEvent(&event);
		SDL_Delay(40);
	}
	thread_exit=0;
	//Break
	SDL_Event event;
	event.type = BREAK_EVENT;
	SDL_PushEvent(&event);

	return 0;
}

int main(int argc, char* argv[])
{
	if(SDL_Init(SDL_INIT_VIDEO)) {  
		printf( "Could not initialize SDL - %s\n", SDL_GetError()); 
		return -1;
	} 

	SDL_Window *screen; 
	//SDL 2.0 Support for multiple windows
	screen = SDL_CreateWindow("Simplest Video Play SDL2", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		screen_w, screen_h,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
	if(!screen) {  
		printf("SDL: could not create window - exiting:%s\n",SDL_GetError());  
		return -1;
	}
	SDL_Renderer* sdlRenderer = SDL_CreateRenderer(screen, -1, 0);  

	Uint32 pixformat=0;

	//IYUV: Y + U + V  (3 planes)
	//YV12: Y + V + U  (3 planes)
	pixformat= SDL_PIXELFORMAT_IYUV;  

	SDL_Texture* sdlTexture = SDL_CreateTexture(sdlRenderer,pixformat, SDL_TEXTUREACCESS_STREAMING,pixel_w,pixel_h);

	FILE *fp=NULL;

#ifdef F_480_272
	fp = fopen("bigbuckbunny_480x272.yuv", "rb+");
	
	//fp = fopen("yuvgradientbar_1280x720_yuv420p.yuv", "rb+");
	//fp = fopen("sintel_480x272_yuv420p.yuv", "rb+");
#else
	fp = fopen("test_yuv420p_320x180.yuv", "rb+");
#endif

	if(fp==NULL){
		printf("cannot open this file\n");
		return -1;
	}

	SDL_Rect sdlRect;  

	SDL_Thread *refresh_thread = SDL_CreateThread(refresh_video,NULL,NULL);
	SDL_Event event;
	while(1){
		//Wait
		SDL_WaitEvent(&event);
		if(event.type==REFRESH_EVENT){
			if (fread(buffer, 1, pixel_w*pixel_h*bpp/8, fp) != pixel_w*pixel_h*bpp/8){
				// Loop
				fseek(fp, 0, SEEK_SET);
				fread(buffer, 1, pixel_w*pixel_h*bpp/8, fp);
			}

			SDL_UpdateTexture( sdlTexture, NULL, buffer, pixel_w);  

			//FIX: If window is resize
			sdlRect.x = 0;  
			sdlRect.y = 0;  
			sdlRect.w = screen_w;  
			sdlRect.h = screen_h;  
			
			SDL_RenderClear( sdlRenderer );   
			SDL_RenderCopy( sdlRenderer, sdlTexture, NULL, &sdlRect);  
			SDL_RenderPresent( sdlRenderer );  
			
		}else if(event.type==SDL_WINDOWEVENT){
			//If Resize
			SDL_GetWindowSize(screen,&screen_w,&screen_h);
		}else if(event.type==SDL_QUIT){
			thread_exit=1;
		}else if(event.type==BREAK_EVENT){
			break;
		}
	}
	SDL_Quit();
	return 0;
}
