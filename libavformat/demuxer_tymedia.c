#include "demuxer_tymedia.h"

static const AVClass demuxer_class = {
	.class_name = "tuya media demuxer",
	.item_name = av_default_item_name,
	.version = LIBAVUTIL_VERSION_INT
};

int tymeida_read_header(AVFormatContext* ctx)
{
	TYMediaDemuxerContext* s = (TYMediaDemuxerContext*)ctx->priv_data;
	enum AVPixelFormat pix_fmt;
	AVStream* st1;
	AVStream* st2;
	AVCodec* codec1 = avcodec_find_decoder(AV_CODEC_ID_H264);
	AVCodec* codec2 = avcodec_find_decoder(AV_CODEC_ID_PCM_S16LE);
	st1 = avformat_new_stream(ctx, codec1);
	st2 = avformat_new_stream(ctx, codec2);
	if (!st1 || !st2)
		return AVERROR(ENOMEM);

	st1->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
	st1->codecpar->codec_id = AV_CODEC_ID_H264;
	st1->time_base.num = 1;
	st1->time_base.den = 1000;

	st2->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
	st2->codecpar->channels = 1;
	st2->codecpar->sample_rate = 8000;
	st2->codecpar->codec_id = AV_CODEC_ID_PCM_S16LE;
	st2->time_base.num = 1;
	st2->time_base.den = 1000;

	return 0;
}

int tymeida_read_packet(AVFormatContext* s, AVPacket* pkt)
{
	int ret;

	STORAGE_FRAME_HEAD_S strHead;
	memset(&strHead, 0x00, sizeof(strHead));

	avio_read(s->pb, &strHead, sizeof(strHead));
	ret = av_get_packet(s->pb, pkt, strHead.size);
	pkt->pts = strHead.timestamp;
	if (strHead.type == 3)
	{
		pkt->stream_index = 1;
	}
	else
	{
		pkt->stream_index = 0;
	}

	if (ret < 0)
		return ret;

	return 0;
}

int tymeida_read_seek(AVFormatContext* s, int stream_index, int64_t timestamp, int flags) {

	AVStream* st;
	int i, index;
	int64_t pos, pos_min;

	st = s->streams[stream_index];

	//对stream_index以时间为顺序查找timestamp在索引表的最适用的位置
	index = av_index_search_timestamp(st,
		timestamp * FFMAX(ast->sample_size, 1),
		flags);

	if (index < 0) {
		if (st->internal->nb_index_entries > 0)
			av_log(s, AV_LOG_DEBUG, "Failed to find timestamp %"PRId64 " in index %"PRId64 " .. %"PRId64 "\n",
				timestamp * FFMAX(ast->sample_size, 1),
				st->internal->index_entries[0].timestamp,
				st->internal->index_entries[st->internal->nb_index_entries - 1].timestamp);
		return AVERROR_INVALIDDATA;
	}

	/* find the position */
	pos = st->internal->index_entries[index].pos;
	timestamp = st->internal->index_entries[index].timestamp / FFMAX(ast->sample_size, 1);

	av_log(s, AV_LOG_TRACE, "XX %"PRId64" %d %"PRId64"\n",
		timestamp, index, st->internal->index_entries[index].timestamp);

	if (CONFIG_DV_DEMUXER && avi->dv_demux) {
		/* One and only one real stream for DV in AVI, and it has video  */
		/* offsets. Calling with other stream indexes should have failed */
		/* the av_index_search_timestamp call above.                     */

		if (avio_seek(s->pb, pos, SEEK_SET) < 0)
			return -1;

		/* Feed the DV video stream version of the timestamp to the */
		/* DV demux so it can synthesize correct timestamps.        */
		ff_dv_offset_reset(avi->dv_demux, timestamp);

		avi->stream_index = -1;
		return 0;
	}

	pos_min = pos;
	for (i = 0; i < s->nb_streams; i++) {
		AVStream* st2 = s->streams[i];
		AVIStream* ast2 = st2->priv_data;

		ast2->packet_size =
			ast2->remaining = 0;

		if (ast2->sub_ctx) {
			seek_subtitle(st, st2, timestamp);
			continue;
		}

		if (st2->internal->nb_index_entries <= 0)
			continue;

		//        av_assert1(st2->codecpar->block_align);
		index = av_index_search_timestamp(st2,
			av_rescale_q(timestamp,
				st->time_base,
				st2->time_base) *
			FFMAX(ast2->sample_size, 1),
			flags |
			AVSEEK_FLAG_BACKWARD |
			(st2->codecpar->codec_type != AVMEDIA_TYPE_VIDEO ? AVSEEK_FLAG_ANY : 0));
		if (index < 0)
			index = 0;
		ast2->seek_pos = st2->internal->index_entries[index].pos;
		pos_min = FFMIN(pos_min, ast2->seek_pos);
	}
	for (i = 0; i < s->nb_streams; i++) {
		AVStream* st2 = s->streams[i];
		AVIStream* ast2 = st2->priv_data;

		if (ast2->sub_ctx || st2->internal->nb_index_entries <= 0)
			continue;

		index = av_index_search_timestamp(
			st2,
			av_rescale_q(timestamp, st->time_base, st2->time_base) * FFMAX(ast2->sample_size, 1),
			flags | AVSEEK_FLAG_BACKWARD | (st2->codecpar->codec_type != AVMEDIA_TYPE_VIDEO ? AVSEEK_FLAG_ANY : 0));
		if (index < 0)
			index = 0;
		while (!avi->non_interleaved && index > 0 && st2->internal->index_entries[index - 1].pos >= pos_min)
			index--;
		ast2->frame_offset = st2->internal->index_entries[index].timestamp;
	}

	/* do the seek */
	if (avio_seek(s->pb, pos_min, SEEK_SET) < 0) {
		av_log(s, AV_LOG_ERROR, "Seek failed\n");
		return -1;
	}
	avi->stream_index = -1;
	avi->dts_max = INT_MIN;
	return 0;
}

#define NULL_IF_CONFIG_SMALL(x) x
AVInputFormat ff_tymedia_demuxer = {
	.name = "media",
	.long_name = NULL_IF_CONFIG_SMALL("TY Media Container"),
	.flags = AVFMT_GENERIC_INDEX,
	.extensions = "media",
	.priv_class = &demuxer_class,
	.raw_codec_id = AV_CODEC_ID_RAWVIDEO,
	.priv_data_size = sizeof(TYMediaDemuxerContext),
	.read_header = tymeida_read_header,
	.read_packet = tymeida_read_packet,
	.read_seek = tymeida_read_seek
};