#ifndef AVFORMAT_TYMEDIA_H  
#define AVFORMAT_TYMEDIA_H  

#include "libavutil/opt.h"  
#include "avformat.h"  

typedef struct TYMediaDemuxerContext {
	const AVClass* pclass;     /**< Class for private options. */
	int width, height;        /**< Integers describing video size, set by a private option. */
	char* pixel_format;       /**< Set by a private option. */
	AVRational framerate;     /**< AVRational describing framerate, set by a private option. */
} TYMediaDemuxerContext;

typedef struct
{
	unsigned int type;
	unsigned int size;
	unsigned long long timestamp;
	unsigned long long pts;
} STORAGE_FRAME_HEAD_S;

int tymeida_read_header(AVFormatContext* ctx);
int tymeida_read_packet(AVFormatContext* s, AVPacket* pkt);

#endif //AVFORMAT_MKDEMUXER_H  