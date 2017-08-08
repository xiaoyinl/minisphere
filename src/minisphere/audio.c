#include "minisphere.h"
#include "audio.h"

struct mixer
{
	unsigned int   refcount;
	unsigned int   id;
	ALLEGRO_MIXER* ptr;
	ALLEGRO_VOICE* voice;
	float          gain;
};

struct stream
{
	unsigned int          refcount;
	unsigned int          id;
	ALLEGRO_AUDIO_STREAM* ptr;
	unsigned char*        buffer;
	size_t                buffer_size;
	size_t                feed_size;
	size_t                fragment_size;
	mixer_t*              mixer;
};

struct sound
{
	unsigned int          refcount;
	unsigned int          id;
	void*                 file_data;
	size_t                file_size;
	float                 gain;
	bool                  is_looping;
	mixer_t*              mixer;
	char*                 path;
	float                 pan;
	float                 pitch;
	bool                  suspended;
	ALLEGRO_AUDIO_STREAM* stream;
};

struct sample
{
	unsigned int    refcount;
	unsigned int    id;
	float           gain;
	float           pan;
	char*           path;
	bool            polyphonic;
	float           speed;
	ALLEGRO_SAMPLE* ptr;
};

struct sample_instance
{
	mixer_t*                 mixer;
	ALLEGRO_SAMPLE_INSTANCE* ptr;
	sample_t*                sample;
};

static bool reload_sound  (sound_t* sound);
static void update_stream (stream_t* stream);

static vector_t*            s_active_samples;
static vector_t*            s_active_sounds;
static vector_t*            s_active_streams;
static ALLEGRO_AUDIO_DEPTH  s_bit_depth;
static ALLEGRO_CHANNEL_CONF s_channel_conf;
static bool                 s_have_sound;
static unsigned int         s_next_mixer_id = 0;
static unsigned int         s_next_sample_id = 0;
static unsigned int         s_next_sound_id = 0;
static unsigned int         s_next_stream_id = 0;
static unsigned int         s_num_refs = 0;

void
audio_init(void)
{
	if (++s_num_refs > 1)
		return;

	console_log(1, "initializing audio subsystem");

	s_have_sound = true;
	if (!al_install_audio()) {
		s_have_sound = false;
		console_log(1, "  audio is not available");
		return;
	}
	al_init_acodec_addon();
	s_active_streams = vector_new(sizeof(stream_t*));
	s_active_samples = vector_new(sizeof(struct sample_instance));
	s_active_sounds = vector_new(sizeof(sound_t*));
}

void
audio_uninit(void)
{
	iter_t iter;
	struct sample_instance *p_sample;
	sound_t*               *p_sound;

	if (--s_num_refs > 0)
		return;

	console_log(1, "shutting down audio subsystem");
	
	iter = vector_enum(s_active_sounds);
	while (p_sound = vector_next(&iter))
		sound_free(*p_sound);
	vector_free(s_active_sounds);
	iter = vector_enum(s_active_samples);
	while (p_sample = vector_next(&iter)) {
		al_destroy_sample_instance(p_sample->ptr);
		sample_free(p_sample->sample);
		mixer_free(p_sample->mixer);
	}
	vector_free(s_active_samples);
	vector_free(s_active_streams);
	if (s_have_sound)
		al_uninstall_audio();
}

void
audio_resume(void)
{
	iter_t iter;
	sound_t*  *p_sound;

	iter = vector_enum(s_active_sounds);
	while (p_sound = vector_next(&iter)) {
		if ((*p_sound)->suspended)
			sound_pause(*p_sound, false);
	}
}

void
audio_suspend(void)
{
	iter_t iter;
	sound_t*  *p_sound;

	iter = vector_enum(s_active_sounds);
	while (p_sound = vector_next(&iter)) {
		(*p_sound)->suspended = sound_playing(*p_sound);
		sound_pause(*p_sound, true);
	}
}

void
audio_update(void)
{
	struct sample_instance *p_sample;
	sound_t*               *p_sound;
	stream_t*              *p_stream;

	iter_t iter;

	if (s_num_refs == 0)
		return;

	iter = vector_enum(s_active_streams);
	while (p_stream = vector_next(&iter))
		update_stream(*p_stream);

	iter = vector_enum(s_active_samples);
	while (p_sample = vector_next(&iter)) {
		if (al_get_sample_instance_playing(p_sample->ptr))
			continue;
		al_destroy_sample_instance(p_sample->ptr);
		sample_free(p_sample->sample);
		mixer_free(p_sample->mixer);
		iter_remove(&iter);
	}
	
	iter = vector_enum(s_active_sounds);
	while (p_sound = vector_next(&iter)) {
		if (sound_playing(*p_sound))
			continue;
		sound_free(*p_sound);
		iter_remove(&iter);
	}
}

mixer_t*
mixer_new(int frequency, int bits, int channels)
{
	ALLEGRO_CHANNEL_CONF conf;
	ALLEGRO_AUDIO_DEPTH  depth;
	mixer_t*             mixer;

	console_log(2, "creating new mixer #%u at %d kHz", s_next_mixer_id, frequency / 1000);
	console_log(3, "    format: %dch %d Hz, %d-bit", channels, frequency, bits);

	conf = channels == 2 ? ALLEGRO_CHANNEL_CONF_2
		: channels == 3 ? ALLEGRO_CHANNEL_CONF_3
		: channels == 4 ? ALLEGRO_CHANNEL_CONF_4
		: channels == 5 ? ALLEGRO_CHANNEL_CONF_5_1
		: channels == 6 ? ALLEGRO_CHANNEL_CONF_6_1
		: channels == 7 ? ALLEGRO_CHANNEL_CONF_7_1
		: ALLEGRO_CHANNEL_CONF_1;
	depth = bits == 16 ? ALLEGRO_AUDIO_DEPTH_INT16
		: bits == 24 ? ALLEGRO_AUDIO_DEPTH_INT24
		: bits == 32 ? ALLEGRO_AUDIO_DEPTH_FLOAT32
		: ALLEGRO_AUDIO_DEPTH_UINT8;

	mixer = calloc(1, sizeof(mixer_t));
	if (!(mixer->voice = al_create_voice(frequency, depth, conf)))
		goto on_error;
	if (!(mixer->ptr = al_create_mixer(frequency, ALLEGRO_AUDIO_DEPTH_FLOAT32, conf)))
		goto on_error;
	al_attach_mixer_to_voice(mixer->ptr, mixer->voice);
	al_set_mixer_gain(mixer->ptr, 1.0);
	al_set_voice_playing(mixer->voice, true);
	al_set_mixer_playing(mixer->ptr, true);

	mixer->gain = al_get_mixer_gain(mixer->ptr);
	mixer->id = s_next_mixer_id++;
	return mixer_ref(mixer);

on_error:
	console_log(2, "failed to create mixer #%u", s_next_mixer_id++);
	if (mixer->ptr != NULL)
		al_destroy_mixer(mixer->ptr);
	if (mixer->voice != NULL)
		al_destroy_voice(mixer->voice);
	free(mixer);
	return NULL;
}

mixer_t*
mixer_ref(mixer_t* mixer)
{
	++mixer->refcount;
	return mixer;
}

void
mixer_free(mixer_t* mixer)
{
	if (mixer == NULL || --mixer->refcount > 0)
		return;

	console_log(3, "disposing mixer #%u no longer in use", mixer->id);
	al_destroy_mixer(mixer->ptr);
	free(mixer);
}

float
mixer_get_gain(mixer_t* mixer)
{
	return mixer->gain;
}

void
mixer_set_gain(mixer_t* mixer, float gain)
{
	al_set_mixer_gain(mixer->ptr, gain);
	mixer->gain = gain;
}

sample_t*
sample_new(const char* path, bool polyphonic)
{
	ALLEGRO_SAMPLE* al_sample;
	ALLEGRO_FILE*   file;
	void*           file_data;
	size_t          file_size;
	sample_t*       sample = NULL;

	console_log(2, "loading sample #%u from `%s`", s_next_sample_id, path);

	if (!(file_data = fs_read_file(g_game_fs, path, NULL, &file_size)))
		goto on_error;
	file = al_open_memfile(file_data, file_size, "rb");
	al_sample = al_load_sample_f(file, strrchr(path, '.'));
	al_fclose(file);

	sample = calloc(1, sizeof(sample_t));
	sample->id = s_next_sample_id++;
	sample->path = strdup(path);
	sample->ptr = al_sample;
	sample->polyphonic = polyphonic;
	sample->gain = 1.0;
	sample->pan = 0.0;
	sample->speed = 1.0;
	return sample_ref(sample);

on_error:
	console_log(2, "    failed to load sample #%u", s_next_sample_id);
	free(sample);
	return NULL;
}

sample_t*
sample_ref(sample_t* sample)
{
	++sample->refcount;
	return sample;
}

void
sample_free(sample_t* sample)
{
	if (sample == NULL || --sample->refcount > 0)
		return;

	console_log(3, "disposing sample #%u no longer in use", sample->id);
	al_destroy_sample(sample->ptr);
	free(sample);
}

const char*
sample_path(const sample_t* sample)
{
	return sample->path;
}

float
sample_get_gain(const sample_t* sample)
{
	return sample->gain;
}

float
sample_get_pan(const sample_t* sample)
{
	return sample->pan;
}

float
sample_get_speed(const sample_t* sample)
{
	return sample->speed;
}

void
sample_set_gain(sample_t* sample, float gain)
{
	sample->gain = gain;
}

void
sample_set_pan(sample_t* sample, float pan)
{
	sample->pan = pan;
}

void
sample_set_speed(sample_t* sample, float speed)
{
	sample->speed = speed;
}

void
sample_play(sample_t* sample, mixer_t* mixer)
{
	struct sample_instance   instance;
	ALLEGRO_SAMPLE_INSTANCE* stream_ptr;
	
	console_log(2, "playing sample #%u on mixer #%u", sample->id, mixer->id);

	if (!sample->polyphonic)
		sample_stop_all(sample);
	stream_ptr = al_create_sample_instance(sample->ptr);
	al_set_sample_instance_gain(stream_ptr, sample->gain);
	al_set_sample_instance_speed(stream_ptr, sample->speed);
	al_set_sample_instance_pan(stream_ptr, sample->pan);
	al_attach_sample_instance_to_mixer(stream_ptr, mixer->ptr);
	al_play_sample_instance(stream_ptr);
	
	instance.sample = sample_ref(sample);
	instance.mixer = mixer_ref(mixer);
	instance.ptr = stream_ptr;
	vector_push(s_active_samples, &instance);
}

void
sample_stop_all(sample_t* sample)
{
	iter_t iter;
	struct sample_instance* p_instance;

	console_log(2, "stopping all instances of sample #%u", sample->id);
	iter = vector_enum(s_active_samples);
	while (p_instance = vector_next(&iter)) {
		if (p_instance->sample != sample)
			continue;
		al_destroy_sample_instance(p_instance->ptr);
		sample_free(p_instance->sample);
		mixer_free(p_instance->mixer);
		iter_remove(&iter);
	}
}

sound_t*
sound_new(const char* path)
{
	sound_t* sound;

	console_log(2, "loading sound #%u from `%s`", s_next_sound_id, path);

	sound = calloc(1, sizeof(sound_t));
	sound->path = strdup(path);

	if (!(sound->file_data = fs_read_file(g_game_fs, sound->path, NULL, &sound->file_size)))
		goto on_error;
	sound->gain = 1.0;
	sound->pan = 0.0;
	sound->pitch = 1.0;
	if (!reload_sound(sound))
		goto on_error;
	sound->id = s_next_sound_id++;
	return sound_ref(sound);

on_error:
	console_log(2, "    failed to load sound #%u", s_next_sound_id);
	if (sound != NULL) {
		free(sound->path);
		free(sound);
	}
	return NULL;
}

sound_t*
sound_ref(sound_t* sound)
{
	++sound->refcount;
	return sound;
}

void
sound_free(sound_t* sound)
{
	if (sound == NULL || --sound->refcount > 0)
		return;

	console_log(3, "disposing sound #%u no longer in use", sound->id);
	free(sound->file_data);
	if (sound->stream != NULL)
		al_destroy_audio_stream(sound->stream);
	mixer_free(sound->mixer);
	free(sound->path);
	free(sound);
}

float
sound_gain(sound_t* sound)
{
	return sound->gain;
}

double
sound_len(sound_t* sound)
{
	if (sound->stream != NULL)
		return al_get_audio_stream_length_secs(sound->stream);
	else
		return 0.0;
}

mixer_t*
sound_mixer(sound_t* sound)
{
	return sound_playing(sound) ? sound->mixer : NULL;
}

const char*
sound_path(const sound_t* sound)
{
	return sound->path;
}

float
sound_pan(sound_t* sound)
{
	return sound->pan;
}

bool
sound_playing(sound_t* sound)
{
	if (sound->stream != NULL)
		return al_get_audio_stream_playing(sound->stream);
	else
		return false;
}

float
sound_speed(sound_t* sound)
{
	return sound->pitch;
}

double
sound_tell(sound_t* sound)
{
	if (sound->stream != NULL)
		return al_get_audio_stream_position_secs(sound->stream);
	else
		return 0.0;
}

bool
sound_repeat(sound_t* sound)
{
	return sound->is_looping;
}

void
sound_set_gain(sound_t* sound, float gain)
{
	if (sound->stream != NULL)
		al_set_audio_stream_gain(sound->stream, gain);
	sound->gain = gain;
}

void
sound_set_repeat(sound_t* sound, bool is_looping)
{
	int play_mode;

	play_mode = is_looping ? ALLEGRO_PLAYMODE_LOOP : ALLEGRO_PLAYMODE_ONCE;
	if (sound->stream != NULL)
		al_set_audio_stream_playmode(sound->stream, play_mode);
	sound->is_looping = is_looping;
}

void
sound_set_pan(sound_t* sound, float pan)
{
	if (sound->stream != NULL)
		al_set_audio_stream_pan(sound->stream, pan);
	sound->pan = pan;
}

void
sound_set_speed(sound_t* sound, float pitch)
{
	if (sound->stream != NULL)
		al_set_audio_stream_speed(sound->stream, pitch);
	sound->pitch = pitch;
}

void
sound_pause(sound_t* sound, bool paused)
{
	if (sound->stream != NULL && sound->mixer != NULL)
		al_set_audio_stream_playing(sound->stream, !paused);
}

void
sound_play(sound_t* sound, mixer_t* mixer)
{
	mixer_t* old_mixer;

	console_log(2, "playing sound #%u on mixer #%u", sound->id, mixer->id);
	if (sound->stream != NULL) {
		old_mixer = sound->mixer;
		sound->mixer = mixer_ref(mixer);
		mixer_free(old_mixer);
		al_rewind_audio_stream(sound->stream);
		al_attach_audio_stream_to_mixer(sound->stream, sound->mixer->ptr);
		al_set_audio_stream_playing(sound->stream, true);
		sound_ref(sound);
		vector_push(s_active_sounds, &sound);
	}
}

void
sound_seek(sound_t* sound, double position)
{
	if (sound->stream != NULL)
		al_seek_audio_stream_secs(sound->stream, position);
}

void
sound_stop(sound_t* sound)
{
	console_log(3, "stopping playback of sound #%s", sound->id);
	if (sound->stream == NULL)
		return;
	al_set_audio_stream_playing(sound->stream, false);
	al_rewind_audio_stream(sound->stream);
	mixer_free(sound->mixer);
	sound->mixer = NULL;
}

stream_t*
stream_new(int frequency, int bits, int channels)
{
	ALLEGRO_CHANNEL_CONF conf;
	ALLEGRO_AUDIO_DEPTH  depth_flag;
	size_t               sample_size;
	stream_t*            stream;

	console_log(2, "creating new stream #%u at %d kHz", s_next_stream_id, frequency / 1000);
	console_log(3, "    format: %dch %d Hz, %d-bit", channels, frequency, bits);

	stream = calloc(1, sizeof(stream_t));

	// create the underlying Allegro stream
	depth_flag = bits == 8 ? ALLEGRO_AUDIO_DEPTH_UINT8
		: bits == 24 ? ALLEGRO_AUDIO_DEPTH_INT24
		: bits == 32 ? ALLEGRO_AUDIO_DEPTH_FLOAT32
		: ALLEGRO_AUDIO_DEPTH_INT16;
	conf = channels == 2 ? ALLEGRO_CHANNEL_CONF_2
		: channels == 3 ? ALLEGRO_CHANNEL_CONF_3
		: channels == 4 ? ALLEGRO_CHANNEL_CONF_4
		: channels == 5 ? ALLEGRO_CHANNEL_CONF_5_1
		: channels == 6 ? ALLEGRO_CHANNEL_CONF_6_1
		: channels == 7 ? ALLEGRO_CHANNEL_CONF_7_1
		: ALLEGRO_CHANNEL_CONF_1;
	if (!(stream->ptr = al_create_audio_stream(4, 1024, frequency, depth_flag, conf)))
		goto on_error;
	al_set_audio_stream_playing(stream->ptr, false);
	al_register_event_source(g_events, al_get_audio_stream_event_source(stream->ptr));

	// allocate an initial stream buffer
	sample_size = bits == 8 ? 1
		: bits == 16 ? 2
		: bits == 24 ? 3
		: bits == 32 ? 4
		: 0;
	stream->fragment_size = 1024 * sample_size;
	stream->buffer_size = frequency * sample_size;  // 1 second
	stream->buffer = malloc(stream->buffer_size);

	stream->id = s_next_stream_id++;
	vector_push(s_active_streams, &stream);
	return stream_ref(stream);

on_error:
	console_log(2, "failed to create stream #%u", s_next_stream_id);
	free(stream);
	return NULL;
}

stream_t*
stream_ref(stream_t* stream)
{
	++stream->refcount;
	return stream;
}

void
stream_free(stream_t* stream)
{
	stream_t* *p_stream;

	iter_t iter;

	if (stream == NULL || --stream->refcount > 0)
		return;

	console_log(3, "disposing stream #%u no longer in use", stream->id);
	al_drain_audio_stream(stream->ptr);
	al_destroy_audio_stream(stream->ptr);
	mixer_free(stream->mixer);
	free(stream->buffer);
	free(stream);
	iter = vector_enum(s_active_streams);
	while (p_stream = vector_next(&iter)) {
		if (*p_stream == stream) {
			vector_remove(s_active_streams, iter.index);
			break;
		}
	}
}

size_t
stream_bytes_left(const stream_t* stream)
{
	return stream->feed_size;
}

mixer_t*
stream_mixer(const stream_t* stream)
{
	return stream->mixer;
}

bool
stream_playing(const stream_t* stream)
{
	return al_get_audio_stream_playing(stream->ptr);
}

void
stream_buffer(stream_t* stream, const void* data, size_t size)
{
	size_t needed_size;

	console_log(4, "buffering %zu bytes into stream #%u", size, stream->id);

	needed_size = stream->feed_size + size;
	if (needed_size > stream->buffer_size) {
		// buffer is too small, double size until large enough
		while (needed_size > stream->buffer_size)
			stream->buffer_size *= 2;
		stream->buffer = realloc(stream->buffer, stream->buffer_size);
	}
	memcpy(stream->buffer + stream->feed_size, data, size);
	stream->feed_size += size;
}

void
stream_pause(stream_t* stream, bool paused)
{
	if (stream->mixer == NULL)
		return;
	al_set_audio_stream_playing(stream->ptr, !paused);
}

void
stream_play(stream_t* stream, mixer_t* mixer)
{
	mixer_t* old_mixer;

	old_mixer = stream->mixer;
	stream->mixer = mixer_ref(mixer);
	mixer_free(old_mixer);

	al_detach_audio_stream(stream->ptr);
	al_attach_audio_stream_to_mixer(stream->ptr, stream->mixer->ptr);
	al_set_audio_stream_playing(stream->ptr, true);
}

void
stream_stop(stream_t* stream)
{
	al_drain_audio_stream(stream->ptr);
	mixer_free(stream->mixer);
	stream->mixer = NULL;
	free(stream->buffer); stream->buffer = NULL;
	stream->feed_size = 0;
}

static bool
reload_sound(sound_t* sound)
{
	// TODO: fold this back into sound_new() as it's not used anywhere else anymore.
	//       it was originally needed to work around a bug where Allegro wouldn't play a
	//       stream that had already played once, but that bug has since been fixed.

	ALLEGRO_FILE*         memfile;
	ALLEGRO_AUDIO_STREAM* new_stream = NULL;
	int                   play_mode;

	new_stream = NULL;
	if (s_have_sound) {
		memfile = al_open_memfile(sound->file_data, sound->file_size, "rb");
		if (!(new_stream = al_load_audio_stream_f(memfile, strrchr(sound->path, '.'), 4, 1024)))
			goto on_error;
	}
	if (s_have_sound && new_stream == NULL)
		return false;
	if (sound->stream != NULL)
		al_destroy_audio_stream(sound->stream);
	if ((sound->stream = new_stream) != NULL) {
		play_mode = sound->is_looping ? ALLEGRO_PLAYMODE_LOOP : ALLEGRO_PLAYMODE_ONCE;
		al_set_audio_stream_gain(sound->stream, sound->gain);
		al_set_audio_stream_pan(sound->stream, sound->pan);
		al_set_audio_stream_speed(sound->stream, sound->pitch);
		al_set_audio_stream_playmode(sound->stream, play_mode);
		al_set_audio_stream_playing(sound->stream, false);
	}
	return true;

on_error:
	return false;
}

static void
update_stream(stream_t* stream)
{
	void*  buffer;

	if (stream->feed_size <= stream->fragment_size) return;
	if (!(buffer = al_get_audio_stream_fragment(stream->ptr)))
		return;
	memcpy(buffer, stream->buffer, stream->fragment_size);
	stream->feed_size -= stream->fragment_size;
	memmove(stream->buffer, stream->buffer + stream->fragment_size, stream->feed_size);
	al_set_audio_stream_fragment(stream->ptr, buffer);
}
