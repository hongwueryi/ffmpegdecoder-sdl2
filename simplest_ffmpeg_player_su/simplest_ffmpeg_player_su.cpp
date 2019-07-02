/**
 * 最简单的基于FFmpeg的视频播放器2(SDL升级版)
 * Simplest FFmpeg Player 2(SDL Update)
 *
 * 雷霄骅 Lei Xiaohua
 * leixiaohua1020@126.com
 * 中国传媒大学/数字电视技术
 * Communication University of China / Digital TV Technology
 * http://blog.csdn.net/leixiaohua1020
 *
 * 第2版使用SDL2.0取代了第一版中的SDL1.2
 * Version 2 use SDL 2.0 instead of SDL 1.2 in version 1.
 *
 * 本程序实现了视频文件的解码和显示(支持HEVC，H.264，MPEG2等)。
 * 是最简单的FFmpeg视频解码方面的教程。
 * 通过学习本例子可以了解FFmpeg的解码流程。
 * 本版本中使用SDL消息机制刷新视频画面。
 * This software is a simplest video player based on FFmpeg.
 * Suitable for beginner of FFmpeg.
 *
 * 备注:
 * 标准版在播放视频的时候，画面显示使用延时40ms的方式。这么做有两个后果：
 * （1）SDL弹出的窗口无法移动，一直显示是忙碌状态
 * （2）画面显示并不是严格的40ms一帧，因为还没有考虑解码的时间。
 * SU（SDL Update）版在视频解码的过程中，不再使用延时40ms的方式，而是创建了
 * 一个线程，每隔40ms发送一个自定义的消息，告知主函数进行解码显示。这样做之后：
 * （1）SDL弹出的窗口可以移动了
 * （2）画面显示是严格的40ms一帧
 * Remark:
 * Standard Version use's SDL_Delay() to control video's frame rate, it has 2
 * disadvantages:
 * (1)SDL's Screen can't be moved and always "Busy".
 * (2)Frame rate can't be accurate because it doesn't consider the time consumed 
 * by avcodec_decode_video2()
 * SU（SDL Update）Version solved 2 problems above. It create a thread to send SDL 
 * Event every 40ms to tell the main loop to decode and show video frames.
 */

#include <stdio.h>

 ///////////////////////////////////////////////////////
 /*
 2019/6/13 宏修改

 因為在嘗試編譯時，會出現以下錯誤：
 1>SDL2main.lib(SDL_windows_main.obj) : error LNK2019: unresolved external symbol __imp__fprintf referenced in function _ShowError
 1>SDL2main.lib(SDL_windows_main.obj) : error LNK2019: unresolved external symbol __imp____iob_func referenced in function _ShowError
 1>E:DocumentsVisual Studio 2015ProjectsSDL2_TestDebugSDL2_Test.exe : fatal error LNK1120: 2 unresolved externals
 错误原因：
 在 Visual Studio 2015中，標準標準，stderr，stdout定義如下：

 #define stdin (__acrt_iob_func(0))
 #define stdout (__acrt_iob_func(1))
 #define stderr (__acrt_iob_func(2))
 但在以前，它們被定義為：

 #define stdin (&__iob_func()[0])
 #define stdout (&__iob_func()[1])
 #define stderr (&__iob_func()[2])
 因這裡現在__iob_func不再定義了，這會導致使用與以前版本的Visual Studio 版本編譯的. lib 文件的鏈接錯誤。

 為了解決這個問題，你可以嘗試定義 __iob_func()，它應該返回一個包含 {*stdin,*stdout,*stderr}的array 。

 關於stdio函數( 在我看來是 sprintf() )的其他鏈接錯誤，你可以將 legacy_stdio_definitions.lib 添加到鏈接器選項。

 或者通過VS2015編譯SDL2源代碼解決该错误
 */
FILE _iob[] = { *stdin, *stdout, *stderr };
extern"C" FILE * __cdecl __iob_func(void)
{
	return _iob;
}
/////////////////////////////////////////////////////////
#include <memory>
#include <Windows.h>
#define __STDC_CONSTANT_MACROS

#ifdef _WIN32
//Windows
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "SDL2/SDL.h"
#include "recorder.h"
};
#else
//Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <SDL2/SDL.h>
#ifdef __cplusplus
};
#endif
#endif

//Refresh Event
#define SFM_REFRESH_EVENT  (SDL_USEREVENT + 1)

#define SFM_BREAK_EVENT  (SDL_USEREVENT + 2)

int thread_exit=0;
int thread_pause=0;

int sfp_refresh_thread(void *opaque){
	thread_exit=0;
	thread_pause=0;

	while (!thread_exit) {
		if(!thread_pause){
			SDL_Event event;
			event.type = SFM_REFRESH_EVENT;
			SDL_PushEvent(&event);
		}
		SDL_Delay(40);
	}
	thread_exit=0;
	thread_pause=0;
	//Break
	SDL_Event event;
	event.type = SFM_BREAK_EVENT;
	SDL_PushEvent(&event);

	return 0;
}


/**
* 将AVFrame(YUV420格式)保存为JPEG格式的图片
*
* @param width YUV420的宽
* @param height YUV42的高
*
*/
int MyWriteJPEG(AVFrame* pFrame, int width, int height, int iIndex)
{
	// 输出文件路径
	char out_file[1024] = { 0 };
	sprintf_s(out_file, sizeof(out_file), "%s%d.jpg", "E:/testjpg/", iIndex);

	// 分配AVFormatContext对象
	AVFormatContext* pFormatCtx = avformat_alloc_context();

	// 设置输出文件格式
	pFormatCtx->oformat = av_guess_format("mjpeg", NULL, NULL);
	// 创建并初始化一个和该url相关的AVIOContext
	if (avio_open(&pFormatCtx->pb, out_file, AVIO_FLAG_READ_WRITE) < 0) {
		printf("Couldn't open output file.");
		return -1;
	}

	// 构建一个新stream
	AVStream* pAVStream = avformat_new_stream(pFormatCtx, 0);
	if (pAVStream == NULL) {
		return -1;
	}

	// 设置该stream的信息
	AVCodecContext* pCodecCtx = pAVStream->codec;

	pCodecCtx->codec_id = pFormatCtx->oformat->video_codec;
	pCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
	pCodecCtx->pix_fmt = AV_PIX_FMT_YUVJ420P;
	pCodecCtx->width = width;
	pCodecCtx->height = height;
	pCodecCtx->time_base.num = 1;
	pCodecCtx->time_base.den = 25;

	// Begin Output some information
	av_dump_format(pFormatCtx, 0, out_file, 1);
	// End Output some information

	// 查找解码器
	AVCodec* pCodec = avcodec_find_encoder(pCodecCtx->codec_id);
	if (!pCodec) {
		printf("Codec not found.");
		return -1;
	}
	// 设置pCodecCtx的解码器为pCodec
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
		printf("Could not open codec.");
		return -1;
	}

	//Write Header
	avformat_write_header(pFormatCtx, NULL);

	int y_size = pCodecCtx->width * pCodecCtx->height;

	//Encode
	// 给AVPacket分配足够大的空间
	AVPacket pkt;
	av_new_packet(&pkt, y_size * 3);

	// 
	int got_picture = 0;
	int ret = avcodec_encode_video2(pCodecCtx, &pkt, pFrame, &got_picture);
	if (ret < 0) {
		printf("Encode Error.\n");
		return -1;
	}
	if (got_picture == 1) {
		//pkt.stream_index = pAVStream->index;
		ret = av_write_frame(pFormatCtx, &pkt);
	}

	av_free_packet(&pkt);

	//Write Trailer
	av_write_trailer(pFormatCtx);

	printf("Encode Successful.\n");

	if (pAVStream) {
		avcodec_close(pAVStream->codec);
	}
	avio_close(pFormatCtx->pb);
	avformat_free_context(pFormatCtx);

	return 0;
}

//保存BMP文件的函数
void SaveAsBMP(AVFrame *pFrameRGB, int width, int height, int index, int bpp)
{
	char buf[5] = { 0 };
	BITMAPFILEHEADER bmpheader;
	BITMAPINFOHEADER bmpinfo;
	FILE *fp;

	char *filename = new char[255];

	//文件存放路径，根据自己的修改
	sprintf_s(filename, 255, "%s%d.bmp", "E:/testbmp/", index);
	if ((fp = fopen(filename, "wb+")) == NULL) {
		printf("open file failed!\n");
		return;
	}

	bmpheader.bfType = 0x4d42;
	bmpheader.bfReserved1 = 0;
	bmpheader.bfReserved2 = 0;
	bmpheader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	bmpheader.bfSize = bmpheader.bfOffBits + width*height*bpp / 8;

	bmpinfo.biSize = sizeof(BITMAPINFOHEADER);
	bmpinfo.biWidth = width;
	bmpinfo.biHeight = height;
	bmpinfo.biPlanes = 1;
	bmpinfo.biBitCount = bpp;
	bmpinfo.biCompression = BI_RGB;
	bmpinfo.biSizeImage = (width*bpp + 31) / 32 * 4 * height;
	bmpinfo.biXPelsPerMeter = 100;
	bmpinfo.biYPelsPerMeter = 100;
	bmpinfo.biClrUsed = 0;
	bmpinfo.biClrImportant = 0;

	fwrite(&bmpheader, sizeof(bmpheader), 1, fp);
	fwrite(&bmpinfo, sizeof(bmpinfo), 1, fp);
	fwrite(pFrameRGB->data[0], width*height*bpp / 8, 1, fp);

	fclose(fp);
}

//void recorder_rescale_packet(AVFormatContext	*pFormatCtx, AVPacket *packet);
//bool recorder_write_header(AVFormatContext	*pFormatCtx, const AVPacket *packet);
//bool recorder_write(AVFormatContext	*pFormatCtx, AVPacket *packet);
int main(int argc, char* argv[])
{

	AVFormatContext	*pFormatCtx;
	int				i, videoindex;
	AVCodecContext	*pCodecCtx;
	AVCodec			*pCodec;
	AVFrame	*pFrame,*pFrameYUV;
	unsigned char *out_buffer;
	AVPacket *packet;
	int ret, got_picture;

	//------------SDL----------------
	int screen_w,screen_h;
	SDL_Window *screen; 
	SDL_Renderer* sdlRenderer;
	SDL_Texture* sdlTexture;
	SDL_Rect sdlRect;
	SDL_Thread *video_tid;
	SDL_Event event;

	struct SwsContext *img_convert_ctx;

	//char filepath[]="Titanic_.ts";
	//char filepath[]="Titanic.ts";
	//char filepath[] = "ds.h264";
	char filepath[] = "bigbuckbunny_480x272.h264";

	av_register_all();
	avformat_network_init();
	pFormatCtx = avformat_alloc_context();

	if(avformat_open_input(&pFormatCtx,filepath,NULL,NULL)!=0){
		printf("Couldn't open input stream.\n");
		return -1;
	}
	if(avformat_find_stream_info(pFormatCtx,NULL)<0){
		printf("Couldn't find stream information.\n");
		return -1;
	}
	videoindex=-1;
	for(i=0; i<pFormatCtx->nb_streams; i++) 
		if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO){
			videoindex=i;
			break;
		}
	if(videoindex==-1){
		printf("Didn't find a video stream.\n");
		return -1;
	}
	pCodecCtx=pFormatCtx->streams[videoindex]->codec;
	pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
	if(pCodec==NULL){
		printf("Codec not found.\n");
		return -1;
	}
	if(avcodec_open2(pCodecCtx, pCodec,NULL)<0){
		printf("Could not open codec.\n");
		return -1;
	}
	pFrame=av_frame_alloc();
	pFrameYUV=av_frame_alloc();

	out_buffer=(unsigned char *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P,  pCodecCtx->width, pCodecCtx->height,1));
	av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize,out_buffer,
		AV_PIX_FMT_YUV420P,pCodecCtx->width, pCodecCtx->height,1);

	//Output Info-----------------------------
	printf("---------------- File Information ---------------\n");
	av_dump_format(pFormatCtx,0,filepath,0);
	printf("-------------------------------------------------\n");
	
	img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, 
		pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL); 
	

	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {  
		printf( "Could not initialize SDL - %s\n", SDL_GetError()); 
		return -1;
	} 
	//SDL 2.0 Support for multiple windows
	screen_w = pCodecCtx->width;
	screen_h = pCodecCtx->height;
	screen = SDL_CreateWindow("Simplest ffmpeg player's Window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		screen_w, screen_h,SDL_WINDOW_OPENGL);

	if(!screen) {  
		printf("SDL: could not create window - exiting:%s\n",SDL_GetError());  
		return -1;
	}
	sdlRenderer = SDL_CreateRenderer(screen, -1, 0);  
	//IYUV: Y + U + V  (3 planes)
	//YV12: Y + V + U  (3 planes)
	sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,pCodecCtx->width,pCodecCtx->height);  

	sdlRect.x=0;
	sdlRect.y=0;
	sdlRect.w=screen_w;
	sdlRect.h=screen_h;

	packet=(AVPacket *)av_malloc(sizeof(AVPacket));

	///////////////////////////////////
	//录屏测试
	static struct recorder recorder;
	struct size declared_frame_size;
	declared_frame_size.width = screen_w;
	declared_frame_size.height = screen_h;
	/*declared_frame_size.width = pCodecCtx->coded_width;
	declared_frame_size.height = pCodecCtx->coded_height;*/
	if (!recorder_init(&recorder, "test.mp4", RECORDER_FORMAT_MP4, declared_frame_size))
	{
		printf("recorder init failed\n");
	}

	if (!recorder_open(&recorder, pCodec))
	{
		printf("recorder open failed\n");
	}
	//////////////////////////////////


	AVFrame* pFrameRGB = av_frame_alloc();
	if (pFrameRGB == NULL) {
		printf("avcodec alloc frame failed!\n");
		return -1;
	}

	
	//////////////////////////////////////////////////////
	//保存bmp图片
	struct SwsContext *pSwsCtx;
	//设置图像转换上下文
	pSwsCtx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
		pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_BGR24,
		SWS_BICUBIC, NULL, NULL, NULL);

	// 确定图片尺寸
	int PictureSize;
	uint8_t *outBuff;
	PictureSize = avpicture_get_size(AV_PIX_FMT_BGR24, pCodecCtx->width, pCodecCtx->height);
	outBuff = (uint8_t*)av_malloc(PictureSize);
	if (outBuff == NULL) {
		printf("av malloc failed!\n");
		return -1;
	}
	avpicture_fill((AVPicture *)pFrameRGB, outBuff, AV_PIX_FMT_BGR24, pCodecCtx->width, pCodecCtx->height);
	//////////////////////////////////////////////////////


	typedef std::shared_ptr<AVFrame> AVFramePtr;
	video_tid = SDL_CreateThread(sfp_refresh_thread,NULL,NULL);
	//------------SDL End------------
	//Event Loop
	bool flag = false;
	for (;;) {
		//Wait
		SDL_WaitEvent(&event);
		if(event.type==SFM_REFRESH_EVENT){
			while(1){
				if (av_read_frame(pFormatCtx, packet) < 0)
				{
					printf("not read\n");
					thread_exit = 1;
				}
				else
				{

				}
				
				if(packet->stream_index==videoindex)
				{
					printf("break=========================\n");
					
					/////////////////////////////
					//录屏测试
					if (!recorder_write(&recorder, packet))
					{
						printf("write failed, pFormatCtx->nb_streams=%d, packet->stream_index=%d\n",
							pFormatCtx->nb_streams, packet->stream_index);
					}
					else
					{
						printf("pFormatCtx->nb_streams=%d, packet->stream_index=%d, packet->pts=%ld, packet->dts=%ld\n",
							pFormatCtx->nb_streams, packet->stream_index, packet->pts, packet->dts);
					}
					//////////////////////////////////////
					
					break;
				}
					
			}	

			ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
			if(ret < 0){
				printf("Decode Error.\n");
				return -1;
			}
			if(got_picture){
#if 0
				AVFrame* pFrameCopy = av_frame_clone(pFrame);
				//保存为图片bmp
				//反转图像 ，否则生成的图像是上下调到的
				pFrameCopy->data[0] += pFrameCopy->linesize[0] * (pCodecCtx->height - 1);
				pFrameCopy->linesize[0] *= -1;
				pFrameCopy->data[1] += pFrameCopy->linesize[1] * (pCodecCtx->height / 2 - 1);
				pFrameCopy->linesize[1] *= -1;
				pFrameCopy->data[2] += pFrameCopy->linesize[2] * (pCodecCtx->height / 2 - 1);
				pFrameCopy->linesize[2] *= -1;
				//转换图像格式，将解压出来的YUV420P的图像转换为BRG24的图像
				sws_scale(pSwsCtx, pFrameCopy->data,
					pFrameCopy->linesize, 0, pCodecCtx->height,
					pFrameRGB->data, pFrameRGB->linesize);
				SaveAsBMP(pFrameRGB, pCodecCtx->width, pCodecCtx->height, i++, 24);
				//av_frame_free(&pFrameCopy);
#endif
#if 0
				static int index = 20;
				//保存为图片jpg
				if (index < 30)
				{
					MyWriteJPEG(pFrame, pCodecCtx->width, pCodecCtx->height, index++);
				}
#endif
				////////////////////////////////
				sws_scale(img_convert_ctx, (const unsigned char* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameYUV->data, pFrameYUV->linesize);
				//SDL---------------------------
				SDL_UpdateTexture( sdlTexture, NULL, pFrameYUV->data[0], pFrameYUV->linesize[0] );  
				SDL_RenderClear( sdlRenderer );  
				//SDL_RenderCopy( sdlRenderer, sdlTexture, &sdlRect, &sdlRect );  
				SDL_RenderCopy( sdlRenderer, sdlTexture, NULL, NULL);  
				SDL_RenderPresent( sdlRenderer );  
				//SDL End-----------------------
			}
			av_free_packet(packet);
		}else if(event.type==SDL_KEYDOWN){
			//Pause
			printf("key down\n");
			if (event.key.keysym.sym == SDLK_SPACE)
			{
				thread_pause = !thread_pause;
			}		
		}
		else if (event.type == SDL_KEYUP) {
			printf("key up\n");
		}
		else if(event.type==SDL_QUIT){
			thread_exit=1;
		}else if(event.type==SFM_BREAK_EVENT){
			break;
		}

	}

	recorder_close(&recorder);
	sws_freeContext(img_convert_ctx);

	SDL_Quit();
	//--------------
	av_frame_free(&pFrameYUV);
	av_frame_free(&pFrame);
	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);

	system("pause");
	return 0;
}