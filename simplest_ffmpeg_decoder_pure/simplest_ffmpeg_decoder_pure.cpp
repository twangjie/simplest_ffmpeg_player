/**
 * 最简单的基于FFmpeg的视频解码器（纯净版）
 * Simplest FFmpeg Decoder Pure
 *
 * 雷霄骅 Lei Xiaohua
 * leixiaohua1020@126.com
 * 中国传媒大学/数字电视技术
 * Communication University of China / Digital TV Technology
 * http://blog.csdn.net/leixiaohua1020
 *
 *
 * 本程序实现了视频码流(支持HEVC，H.264，MPEG2等)解码为YUV数据。
 * 它仅仅使用了libavcodec（而没有使用libavformat）。
 * 是最简单的FFmpeg视频解码方面的教程。
 * 通过学习本例子可以了解FFmpeg的解码流程。
 * This software is a simplest decoder based on FFmpeg.
 * It decode bitstreams to YUV pixel data.
 * It just use libavcodec (do not contains libavformat).
 * Suitable for beginner of FFmpeg.
 */

#include <stdio.h>

#define __STDC_CONSTANT_MACROS

#ifdef _WIN32
//Windows
extern "C"
{
#include "libavcodec/avcodec.h"
};
#else
//Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#ifdef __cplusplus
};
#endif
#endif

#include <queue>


//test different codec
#define TEST_H264  1
#define TEST_HEVC  0

AVCodec *pCodec;
AVCodecContext *pCodecCtx = NULL;
AVCodecParserContext *pCodecParserCtx = NULL;
AVFrame	*pFrame;

const int in_buffer_size = 256 * 1024;
unsigned char in_buffer[in_buffer_size + AV_INPUT_BUFFER_PADDING_SIZE] = { 0 };
unsigned char *cur_ptr;
int cur_size;
AVPacket packet;
int ret, got_picture;

int total_picture_got = 0;
int first_time = 1;

std::queue<unsigned char *> picQueue;

int init(int width, int height);

//FILE *fp_out_in_decode;
int decode(const char *buffer, int size);

unsigned char *get_frame();

void ffmpeg_dump_yuv(char *filename, AVFrame *frame);

void ffmpeg_extract_yuv(unsigned char *destBuffer, AVFrame *frame);

int main(int argc, char* argv[])
{
    FILE *fp_in;
	FILE *fp_out;
 
#if TEST_HEVC
	enum AVCodecID codec_id=AV_CODEC_ID_HEVC;
	char * filepath_in="bigbuckbunny_480x272.hevc";
#elif TEST_H264
	AVCodecID codec_id=AV_CODEC_ID_H264;
	char * filepath_in="bigbuckbunny_480x272.h264";
#else
	AVCodecID codec_id=AV_CODEC_ID_MPEG2VIDEO;
	char * filepath_in="bigbuckbunny_480x272.m2v";
#endif
	
	char *filepath_out = "bigbuckbunny_480x272.yuv";

	int yuvExpected = 0;

	switch (argc)
	{
	case 4:
		yuvExpected = atoi(argv[3]);
	case 3:
		filepath_out = argv[2];
	case 2:
		filepath_in = argv[1];
		break;
	}

	init(800, 600);

	//Input File
    fp_in = fopen(filepath_in, "rb");
    if (!fp_in) {
        printf("Could not open input stream\n");
        return -1;
    }
	//Output File
	fp_out = fopen(filepath_out, "wb");
	if (!fp_out) {
		printf("Could not open output YUV file\n");
		return -1;
	}

	const int fileBufSize = in_buffer_size;
	char fileBuf[fileBufSize];
	int imageSize = 0;
	unsigned char *yuvData = NULL;

	while (yuvExpected == 0 || yuvExpected > total_picture_got) {

        cur_size = fread(fileBuf, 1, fileBufSize, fp_in);
        if (cur_size == 0)
            break;
      
		if(decode(fileBuf, fileBufSize))
		{
			while((yuvData = get_frame()) != NULL)
			{
				total_picture_got++;

				if(imageSize == 0)
				{
					imageSize = pCodecCtx->width * pCodecCtx->height * 3 / 2;
				}

				fwrite(yuvData, 1, imageSize, fp_out);

				printf("Succeed to decode %d frame!\n", total_picture_got);

				delete[] yuvData;

				if(yuvExpected <= total_picture_got)
				{
					break;
				}
			}						
		}
    }
	

    fclose(fp_in);
	fclose(fp_out);
//	if(fp_out_in_decode)
//	{
//		fclose(fp_out_in_decode);
//	}

	av_parser_close(pCodecParserCtx);

	av_frame_free(&pFrame);
	avcodec_close(pCodecCtx);
	av_free(pCodecCtx);

	system("PAUSE");

	return 0;
}

int init(int width, int height)
{
	av_log_set_level(AV_LOG_DEBUG);

	avcodec_register_all();

	//pCodec = avcodec_find_decoder(codec_id);
	pCodec = avcodec_find_decoder_by_name("h264");
	if (!pCodec) {
		printf("Codec not found\n");
		return -1;
	}
	pCodecCtx = avcodec_alloc_context3(pCodec);
	if (!pCodecCtx) {
		printf("Could not allocate video codec context\n");
		return -1;
	}

	pCodecParserCtx = av_parser_init(pCodec->id);
	if (!pCodecParserCtx) {
		printf("Could not allocate video parser context\n");
		return -1;
	}

	//if(pCodec->capabilities&CODEC_CAP_TRUNCATED)
	//    pCodecCtx->flags|= CODEC_FLAG_TRUNCATED; 

	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
		printf("Could not open codec\n");
		return -1;
	}

	pFrame = av_frame_alloc();
	av_init_packet(&packet);

	return 0;
}

int decode(const char *buffer, int size)
{
	if(size <= 0)
	{
		return 0;
	}

	//memcpy(in_buffer, buffer, size);

	cur_ptr = (unsigned char *)buffer;

	int decoded_frames = 0;
	int err = 0;

	while (cur_size>0 && err == 0)
	{
		int len = av_parser_parse2(
			pCodecParserCtx, pCodecCtx,
			&packet.data, &packet.size,
			cur_ptr, cur_size,
			AV_NOPTS_VALUE, AV_NOPTS_VALUE, AV_NOPTS_VALUE);

		cur_ptr += len;
		cur_size -= len;

		if (packet.size == 0)
		{
			break;
		}

		//Some Info from AVCodecParserContext
		printf("[Packet]Size:%6d\t", packet.size);
		switch (pCodecParserCtx->pict_type) {
		case AV_PICTURE_TYPE_I: printf("Type:I\t"); break;
		case AV_PICTURE_TYPE_P: printf("Type:P\t"); break;
		case AV_PICTURE_TYPE_B: printf("Type:B\t"); break;
		default: printf("Type:Other\t"); break;
		}
		printf("Number:%4d\n", pCodecParserCtx->output_picture_number);

		//ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, &packet);
		ret = avcodec_send_packet(pCodecCtx, &packet);
		if (ret != 0)
		{
			printf("Decode Error.\n");
			err = ret;
			break;
		}
		
		while (ret >= 0)
		{
			ret = avcodec_receive_frame(pCodecCtx, pFrame);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			{
				continue;
			}
			else if (ret < 0) 
			{
				fprintf(stderr, "Error during decoding\n");
				err = ret;
				break;
			}

			if (first_time) {
				printf("\nCodec Full Name:%s\n", pCodecCtx->codec->long_name);
				printf("width:%d\nheight:%d\n\n", pCodecCtx->width, pCodecCtx->height);
				first_time = 0;
			}

			unsigned char *yuvData = new unsigned char[pCodecCtx->width * pCodecCtx->height * 3 / 2];

			ffmpeg_extract_yuv(yuvData, pFrame);

			picQueue.push(yuvData);

			decoded_frames++;
		}
	}

	return decoded_frames;
}

unsigned char *get_frame()
{
	if(picQueue.empty())
	{
		return NULL;
	}

	unsigned char *ret = picQueue.front();
	if(ret)
	{
		picQueue.pop();
	}
	
	return ret;
}

void ffmpeg_dump_yuv(char *filename, AVFrame *frame)
{
	FILE *fp = 0;
	int i, j, shift;

	fp = fopen(filename, "wb");
	if (fp) {
		for (i = 0; i < 3; i++) {
			shift = (i == 0 ? 0 : 1);
			unsigned char *yuv_factor = frame->data[i];
			for (j = 0; j < (frame->height >> shift); j++) {
				fwrite(yuv_factor, (frame->width >> shift), 1, fp);
				yuv_factor += frame->linesize[i];
			}
		}
		fclose(fp);
	}
}

void ffmpeg_extract_yuv(unsigned char * destBuffer, AVFrame *frame)
{
	unsigned char *pos = destBuffer;

	for (int i = 0; i < 3; i++) 
	{
		int shift = (i == 0 ? 0 : 1);
		int width_shifted = frame->width >> shift;

		unsigned char *yuv_factor = frame->data[i];
		for (int j = 0; j < (frame->height >> shift); j++)
		{
			memcpy(pos, yuv_factor, width_shifted);
			pos += width_shifted;
			yuv_factor += frame->linesize[i];
		}
	}
}
