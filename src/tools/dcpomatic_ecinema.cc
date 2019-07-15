/*
    Copyright (C) 2018-2019 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "lib/version.h"
#include "lib/decrypted_ecinema_kdm.h"
#include "lib/config.h"
#include "lib/util.h"
#include "lib/film.h"
#include "lib/dcp_content.h"
#include "lib/job_manager.h"
#include "lib/cross.h"
#include "lib/transcode_job.h"
#include "lib/ffmpeg_encoder.h"
#include "lib/signal_manager.h"
#include <dcp/key.h>
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/aes_ctr.h>
}
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <openssl/rand.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string>
#include <iostream>

using std::string;
using std::cerr;
using std::cout;
using std::ofstream;
using boost::optional;
using boost::shared_ptr;

static void convert_dcp (boost::filesystem::path input, boost::filesystem::path output_file, boost::optional<boost::filesystem::path> kdm, int crf);
static void convert_ffmpeg (boost::filesystem::path input, boost::filesystem::path output_file, string format, boost::optional<boost::filesystem::path> kdm);
static void write_kdm (string id, string name, dcp::Key key, optional<boost::filesystem::path> kdm);

static void
help (string n)
{
	cerr << "Syntax: " << n << " [OPTION] <FILE>\n"
	     << "  -v, --version        show DCP-o-matic version\n"
	     << "  -h, --help           show this help\n"
	     << "  -o, --output         output directory\n"
	     << "  -f, --format         output format (mov or mp4; defaults to mov)\n"
	     << "  -k, --kdm            KDM output filename (defaults to stdout)\n"
	     << "  -c, --crf            quality (CRF) when transcoding from DCP (0 is best, 51 is worst, defaults to 23)\n"
	     << "\n"
	     << "<FILE> is the unencrypted .mp4 file.\n";
}

int
main (int argc, char* argv[])
{
	optional<boost::filesystem::path> output;
	optional<string> format;
	optional<boost::filesystem::path> kdm;
	int crf = 23;

	int option_index = 0;
	while (true) {
		static struct option long_options[] = {
			{ "version", no_argument, 0, 'v' },
			{ "help", no_argument, 0, 'h' },
			{ "output", required_argument, 0, 'o' },
			{ "format", required_argument, 0, 'f' },
			{ "kdm", required_argument, 0, 'k' },
			{ "crf", required_argument, 0, 'c' },
		};

		int c = getopt_long (argc, argv, "vho:f:k:c:", long_options, &option_index);

		if (c == -1) {
			break;
		}

		switch (c) {
		case 'v':
			cout << "dcpomatic version " << dcpomatic_version << " " << dcpomatic_git_commit << "\n";
			exit (EXIT_SUCCESS);
		case 'h':
			help (argv[0]);
			exit (EXIT_SUCCESS);
		case 'o':
			output = optarg;
			break;
		case 'f':
			format = optarg;
			break;
		case 'k':
			kdm = optarg;
			break;
		case 'c':
			crf = atoi(optarg);
			break;
		}
	}

	if (optind >= argc) {
		help (argv[0]);
		exit (EXIT_FAILURE);
	}

	if (!output) {
		cerr << "You must specify --output-media or -o\n";
		exit (EXIT_FAILURE);
	}

	if (!format) {
		format = "mov";
	}

	if (*format != "mov" && *format != "mp4") {
		cerr << "Invalid format specified: must be mov or mp4\n";
		exit (EXIT_FAILURE);
	}

	dcpomatic_setup_path_encoding ();
	dcpomatic_setup ();
	signal_manager = new SignalManager ();

	boost::filesystem::path input = argv[optind];
	boost::filesystem::path output_file;
	if (boost::filesystem::is_directory(input)) {
		output_file = *output / (input.parent_path().filename().string() + ".ecinema");
	} else {
		output_file = *output / (input.filename().string() + ".ecinema");
	}

	if (!boost::filesystem::is_directory(*output)) {
		boost::filesystem::create_directory (*output);
	}

	av_register_all ();

	if (boost::filesystem::is_directory(input)) {
		/* Assume input is a DCP */
		convert_dcp (input, output_file, kdm, crf);
	} else {
		convert_ffmpeg (input, output_file, *format, kdm);
	}
}

static void
convert_ffmpeg (boost::filesystem::path input, boost::filesystem::path output_file, string format, optional<boost::filesystem::path> kdm)
{
	AVFormatContext* input_fc = avformat_alloc_context ();
	if (avformat_open_input(&input_fc, input.string().c_str(), 0, 0) < 0) {
		cerr << "Could not open input file\n";
		exit (EXIT_FAILURE);
	}

	if (avformat_find_stream_info (input_fc, 0) < 0) {
		cerr << "Could not read stream information\n";
		exit (EXIT_FAILURE);
	}

	AVFormatContext* output_fc;
	avformat_alloc_output_context2 (&output_fc, av_guess_format(format.c_str(), 0, 0), 0, 0);

	for (uint32_t i = 0; i < input_fc->nb_streams; ++i) {
		AVStream* is = input_fc->streams[i];
		AVStream* os = avformat_new_stream (output_fc, is->codec->codec);
		if (avcodec_parameters_copy (os->codecpar, is->codecpar) < 0) {
			cerr << "Could not set up output stream.\n";
			exit (EXIT_FAILURE);
		}

		os->avg_frame_rate = is->avg_frame_rate;

		switch (is->codec->codec_type) {
		case AVMEDIA_TYPE_VIDEO:
			os->time_base = is->time_base;
			os->r_frame_rate = is->r_frame_rate;
			os->sample_aspect_ratio = is->sample_aspect_ratio;
			os->codec->time_base = is->codec->time_base;
			os->codec->framerate = is->codec->framerate;
			os->codec->pix_fmt = is->codec->pix_fmt;
			break;
		case AVMEDIA_TYPE_AUDIO:
			os->codec->sample_fmt = is->codec->sample_fmt;
			os->codec->bits_per_raw_sample = is->codec->bits_per_raw_sample;
			os->codec->sample_rate = is->codec->sample_rate;
			os->codec->channel_layout = is->codec->channel_layout;
			os->codec->channels = is->codec->channels;
			if (is->codecpar->codec_id == AV_CODEC_ID_PCM_S24LE) {
				/* XXX: fix incoming 24-bit files labelled lpcm, which apparently isn't allowed */
				os->codecpar->codec_tag = MKTAG('i', 'n', '2', '4');
			}
			break;
		default:
			/* XXX */
			break;
		}
	}

	if (avio_open2 (&output_fc->pb, output_file.string().c_str(), AVIO_FLAG_WRITE, 0, 0) < 0) {
		cerr << "Could not open output file `" << output_file.string() << "'\n";
		exit (EXIT_FAILURE);
	}

	dcp::Key key (AES_CTR_KEY_SIZE);
	AVDictionary* options = 0;
	av_dict_set (&options, "encryption_key", key.hex().c_str(), 0);
	/* XXX: is this OK? */
	av_dict_set (&options, "encryption_kid", "00000000000000000000000000000000", 0);
	av_dict_set (&options, "encryption_scheme", "cenc-aes-ctr", 0);

	string id = dcp::make_uuid ();
	if (av_dict_set(&output_fc->metadata, SWAROOP_ID_TAG, id.c_str(), 0) < 0) {
		cerr << "Could not write ID to output.\n";
		exit (EXIT_FAILURE);
	}

	if (avformat_write_header (output_fc, &options) < 0) {
		cerr << "Could not write header to output.\n";
		exit (EXIT_FAILURE);
	}

	AVPacket packet;
	while (av_read_frame(input_fc, &packet) >= 0) {
		AVStream* is = input_fc->streams[packet.stream_index];
		AVStream* os = output_fc->streams[packet.stream_index];
		packet.pts = av_rescale_q_rnd(packet.pts, is->time_base, os->time_base, (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		packet.dts = av_rescale_q_rnd(packet.dts, is->time_base, os->time_base, (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		packet.duration = av_rescale_q(packet.duration, is->time_base, os->time_base);
		packet.pos = -1;
		if (av_interleaved_write_frame (output_fc, &packet) < 0) {
			cerr << "Could not write frame to output.\n";
			exit (EXIT_FAILURE);
		}
	}

	av_write_trailer (output_fc);

	avformat_free_context (input_fc);
	avformat_free_context (output_fc);

	write_kdm (id, output_file.filename().string(), key, kdm);
}

static void
write_kdm (string id, string name, dcp::Key key, optional<boost::filesystem::path> kdm)
{
	DecryptedECinemaKDM decrypted_kdm (id, name, key, optional<dcp::LocalTime>(), optional<dcp::LocalTime>());
	EncryptedECinemaKDM encrypted_kdm = decrypted_kdm.encrypt (Config::instance()->decryption_chain()->leaf());

	if (kdm) {
		ofstream f(kdm->c_str());
		f << encrypted_kdm.as_xml() << "\n";
	} else {
		cout << encrypted_kdm.as_xml() << "\n";
	}
}

static void
convert_dcp (boost::filesystem::path input, boost::filesystem::path output_file, optional<boost::filesystem::path> kdm, int crf)
{
	shared_ptr<Film> film (new Film(boost::optional<boost::filesystem::path>()));
	shared_ptr<DCPContent> dcp (new DCPContent(input));
	film->examine_and_add_content (dcp);

	JobManager* jm = JobManager::instance ();
	while (jm->work_to_do ()) {
		while (signal_manager->ui_idle ()) {}
		dcpomatic_sleep (1);
	}
	DCPOMATIC_ASSERT (!jm->errors());

	string id = dcp::make_uuid ();
	dcp::Key key (AES_CTR_KEY_SIZE);

	shared_ptr<TranscodeJob> job (new TranscodeJob(film));
	job->set_encoder (
		shared_ptr<FFmpegEncoder>(
			new FFmpegEncoder(film, job, output_file, EXPORT_FORMAT_H264, false, false, crf, key, id)
			)
		);
	jm->add (job);
	show_jobs_on_console (true);

	write_kdm (id, output_file.filename().string(), key, kdm);
}
