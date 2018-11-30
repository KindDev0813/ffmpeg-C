#include "AudioOutputStream.h"
#include "FFmpegException.h"

using namespace std;

namespace ffmpegcpp
{
	AudioOutputStream::AudioOutputStream(OpenCodec* codec)
		: OutputStream(codec)
	{

		// if the codec is not a video codec, we are doing it wrong!
		if (codec->GetContext()->codec->type != AVMEDIA_TYPE_AUDIO)
		{
			throw FFmpegException("A video output stream must be initialized with a video codec");
		}
	}

	void AudioOutputStream::OpenStream(AVFormatContext* containerContext)
	{
		this->containerContext = containerContext;

		stream = avformat_new_stream(containerContext, NULL);
		if (!stream)
		{
			throw FFmpegException("Could not allocate stream for container " + string(containerContext->oformat->name));
		}

		stream->id = containerContext->nb_streams - 1;

		// set the time base to the sample rate
		AVRational time_base;
		time_base.num = 1;
		time_base.den = codec->GetContext()->sample_rate;
		stream->time_base = time_base;

		// special case for certain containers
		if (containerContext->oformat->flags & AVFMT_GLOBALHEADER)
			codec->GetContext()->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

		/* copy the stream parameters to the muxer */
		int ret = avcodec_parameters_from_context(stream->codecpar, codec->GetContext());
		if (ret < 0)
		{
			throw FFmpegException("Could not copy codec parameters to stream", ret);
		}
	}

	void AudioOutputStream::WritePacket(AVPacket *pkt)
	{
		/* rescale output packet timestamp values from codec to stream timebase */
		AVRational* time_base = &codec->GetContext()->time_base;
		av_packet_rescale_ts(pkt, *time_base, stream->time_base);
		pkt->stream_index = stream->index;

		// We NEED to fill in the duration here, otherwise the frame rate is calculated wrong in the end for certain codecs/containers (ie h264/mp4).
		if (stream->time_base.num != 0 && stream->avg_frame_rate.num != 0)
		{
			pkt->duration = stream->time_base.den / stream->time_base.num / stream->avg_frame_rate.num * stream->avg_frame_rate.den;
		}

		/* Write the compressed frame to the media file. */
		int ret = av_interleaved_write_frame(containerContext, pkt);
		if (ret < 0)
		{
			throw FFmpegException("Error while writing frame to output container", ret);
		}
	}
}
