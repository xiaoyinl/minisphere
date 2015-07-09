#include "minisphere.h"
#include "api.h"
#include "bytearray.h"

#include "audialis.h"

static duk_ret_t js_LoadSound                  (duk_context* ctx);
static duk_ret_t js_new_Sound                  (duk_context* ctx);
static duk_ret_t js_Sound_finalize             (duk_context* ctx);
static duk_ret_t js_Sound_toString             (duk_context* ctx);
static duk_ret_t js_Sound_get_length           (duk_context* ctx);
static duk_ret_t js_Sound_get_pan              (duk_context* ctx);
static duk_ret_t js_Sound_set_pan              (duk_context* ctx);
static duk_ret_t js_Sound_get_pitch            (duk_context* ctx);
static duk_ret_t js_Sound_set_pitch            (duk_context* ctx);
static duk_ret_t js_Sound_get_playing          (duk_context* ctx);
static duk_ret_t js_Sound_get_position         (duk_context* ctx);
static duk_ret_t js_Sound_set_position         (duk_context* ctx);
static duk_ret_t js_Sound_get_repeat           (duk_context* ctx);
static duk_ret_t js_Sound_set_repeat           (duk_context* ctx);
static duk_ret_t js_Sound_get_seekable         (duk_context* ctx);
static duk_ret_t js_Sound_get_volume           (duk_context* ctx);
static duk_ret_t js_Sound_set_volume           (duk_context* ctx);
static duk_ret_t js_Sound_clone                (duk_context* ctx);
static duk_ret_t js_Sound_pause                (duk_context* ctx);
static duk_ret_t js_Sound_play                 (duk_context* ctx);
static duk_ret_t js_Sound_reset                (duk_context* ctx);
static duk_ret_t js_Sound_stop                 (duk_context* ctx);
static duk_ret_t js_new_SoundStream            (duk_context* ctx);
static duk_ret_t js_SoundStream_finalize       (duk_context* ctx);
static duk_ret_t js_SoundStream_get_bufferSize (duk_context* ctx);
static duk_ret_t js_SoundStream_buffer         (duk_context* ctx);
static duk_ret_t js_SoundStream_play           (duk_context* ctx);
static duk_ret_t js_SoundStream_pause          (duk_context* ctx);
static duk_ret_t js_SoundStream_stop           (duk_context* ctx);

static void update_stream (stream_t* stream);

struct sound
{
	int                   refcount;
	unsigned int          id;
	void*                 file_data;
	size_t                file_size;
	float                 gain;
	bool                  is_looping;
	char*                 path;
	float                 pan;
	float                 pitch;
	ALLEGRO_AUDIO_STREAM* stream;
};

struct stream
{
	unsigned int          refcount;
	ALLEGRO_AUDIO_STREAM* al_stream;
	size_t                fragment_size;
	unsigned char*        buffer;
	size_t                buffer_size;
	size_t                feed_size;
};

static bool         s_have_sound = false;
static unsigned int s_next_sound_id = 0;
static vector_t*    s_streams;

void
initialize_audialis(void)
{
	console_log(1, "Initializing Audialis\n");
	
	if (!(s_have_sound = al_install_audio())) {
		console_log(1, "  Audio initialization failed\n");
		return;
	}
	al_init_acodec_addon();
	al_reserve_samples(10);
	al_set_mixer_gain(al_get_default_mixer(), 1.0);
	s_streams = new_vector(sizeof(stream_t*));
}

void
shutdown_audialis(void)
{
	console_log(1, "Shutting down Audialis\n");
	free_vector(s_streams);
	if (s_have_sound)
		al_uninstall_audio();
}

void
update_audialis(void)
{
	stream_t* *p_stream;
	
	iter_t iter;

	iter = iterate_vector(s_streams);
	while (p_stream = next_vector_item(&iter))
		update_stream(*p_stream);
}

sound_t*
load_sound(const char* path, bool streaming)
{
	sound_t* sound;

	if (!(sound = calloc(1, sizeof(sound_t)))) goto on_error;
	if (!(sound->path = strdup(path))) goto on_error;
	if (!(sound->file_data = sfs_fslurp(g_fs, sound->path, "sounds", &sound->file_size)))
		goto on_error;
	sound->gain = 1.0;
	sound->pan = 0.0;
	sound->pitch = 1.0;
	if (!reload_sound(sound))
		goto on_error;
	sound->id = s_next_sound_id++;
	return ref_sound(sound);

on_error:
	if (sound != NULL) {
		free(sound->path);
		free(sound);
	}
	return NULL;
}

sound_t*
ref_sound(sound_t* sound)
{
	++sound->refcount;
	return sound;
}

void
free_sound(sound_t* sound)
{
	if (sound == NULL || --sound->refcount > 0)
		return;
	free(sound->file_data);
	if (sound->stream != NULL)
		al_destroy_audio_stream(sound->stream);
	free(sound->path);
}

bool
is_sound_looping(sound_t* sound)
{
	if (sound->stream != NULL)
		return al_get_audio_stream_playmode(sound->stream) == ALLEGRO_PLAYMODE_LOOP;
	else
		return false;

}

bool
is_sound_playing(sound_t* sound)
{
	if (sound->stream != NULL)
		return al_get_audio_stream_playing(sound->stream);
	else
		return false;
}

float
get_sound_gain(sound_t* sound)
{
	if (sound->stream != NULL)
		return al_get_audio_stream_gain(sound->stream);
	else
		return 1.0;
}

long long
get_sound_length(sound_t* sound)
{
	if (sound->stream != NULL)
		return al_get_audio_stream_length_secs(sound->stream) * 1000000;
	else
		return 0.0;
}

float
get_sound_pan(sound_t* sound)
{
	if (sound->stream != NULL)
		return al_get_audio_stream_pan(sound->stream);
	else
		return 0.0;
}

float
get_sound_pitch(sound_t* sound)
{
	if (sound->stream != NULL)
		return al_get_audio_stream_speed(sound->stream);
	else
		return 1.0;
}

long long
get_sound_seek(sound_t* sound)
{
	if (sound->stream != NULL)
		return al_get_audio_stream_position_secs(sound->stream) * 1000000;
	else
		return 0.0;
}

void
set_sound_gain(sound_t* sound, float gain)
{
	if (sound->stream != NULL)
		al_set_audio_stream_gain(sound->stream, gain);
	sound->gain = gain;
}

void
set_sound_looping(sound_t* sound, bool is_looping)
{
	int play_mode;

	play_mode = is_looping ? ALLEGRO_PLAYMODE_LOOP : ALLEGRO_PLAYMODE_ONCE;
	if (sound->stream != NULL)
		al_set_audio_stream_playmode(sound->stream, play_mode);
	sound->is_looping = is_looping;
}

void
set_sound_pan(sound_t* sound, float pan)
{
	if (sound->stream != NULL)
		al_set_audio_stream_pan(sound->stream, pan);
	sound->pan = pan;
}

void
set_sound_pitch(sound_t* sound, float pitch)
{
	if (sound->stream != NULL)
		al_set_audio_stream_speed(sound->stream, pitch);
	sound->pitch = pitch;
}

void
play_sound(sound_t* sound)
{
	if (sound->stream != NULL)
		al_set_audio_stream_playing(sound->stream, true);
}

bool
reload_sound(sound_t* sound)
{
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
		al_attach_audio_stream_to_mixer(sound->stream, al_get_default_mixer());
		al_set_audio_stream_playing(sound->stream, false);
	}
	return true;

on_error:
	return false;
}

void
seek_sound(sound_t* sound, long long position)
{
	if (sound->stream != NULL)
		al_seek_audio_stream_secs(sound->stream, (double)position / 1000000);
}

void
stop_sound(sound_t* sound, bool rewind)
{
	if (sound->stream == NULL)
		return;
	al_set_audio_stream_playing(sound->stream, false);
	if (rewind)
		al_rewind_audio_stream(sound->stream);
}

stream_t*
create_stream(int frequency, int bits)
{
	ALLEGRO_AUDIO_DEPTH depth_flag;
	size_t              sample_size;
	stream_t*           stream;

	stream = calloc(1, sizeof(stream_t));
	
	// create the underlying Allegro stream
	depth_flag = bits == 8 ? ALLEGRO_AUDIO_DEPTH_UINT8
		: bits == 24 ? ALLEGRO_AUDIO_DEPTH_INT24
		: ALLEGRO_AUDIO_DEPTH_INT16;
	if (!(stream->al_stream = al_create_audio_stream(4, 1024, frequency, depth_flag, ALLEGRO_CHANNEL_CONF_1)))
		goto on_error;
	al_set_audio_stream_playing(stream->al_stream, false);
	al_attach_audio_stream_to_mixer(stream->al_stream, al_get_default_mixer());

	// allocate an initial stream buffer
	sample_size = bits == 8 ? 1 : bits == 16 ? 2 : bits == 24 ? 3 : 0;
	stream->fragment_size = 1024 * sample_size;
	stream->buffer_size = frequency * sample_size;  // 1 second
	stream->buffer = malloc(stream->buffer_size);
	
	push_back_vector(s_streams, &stream);
	return ref_stream(stream);

on_error:
	free(stream);
	return NULL;
}

stream_t*
ref_stream(stream_t* stream)
{
	++stream->refcount;
	return stream;
}

void
free_stream(stream_t* stream)
{
	stream_t* *p_stream;
	
	iter_t iter;
	
	if (stream == NULL || --stream->refcount > 0)
		return;
	al_drain_audio_stream(stream->al_stream);
	al_destroy_audio_stream(stream->al_stream);
	free(stream->buffer);
	free(stream);
	iter = iterate_vector(s_streams);
	while (p_stream = next_vector_item(&iter)) {
		if (*p_stream == stream) {
			remove_vector_item(s_streams, iter.index);
			break;
		}
	}
}

void
feed_stream(stream_t* stream, const void* data, size_t size)
{
	size_t needed_size;
	
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
pause_stream(stream_t* stream)
{
	al_set_audio_stream_playing(stream->al_stream, false);
}

void
play_stream(stream_t* stream)
{
	al_set_audio_stream_playing(stream->al_stream, true);
}

void
stop_stream(stream_t* stream)
{
	al_drain_audio_stream(stream->al_stream);
	free(stream->buffer); stream->buffer = NULL;
	stream->feed_size = 0;
}

static void
update_stream(stream_t* stream)
{
	void*  buffer;

	if (stream->feed_size <= stream->fragment_size) return;
	if (!(buffer = al_get_audio_stream_fragment(stream->al_stream)))
		return;
	memcpy(buffer, stream->buffer, stream->fragment_size);
	stream->feed_size -= stream->fragment_size;
	memmove(stream->buffer, stream->buffer + stream->fragment_size, stream->feed_size);
	al_set_audio_stream_fragment(stream->al_stream, buffer);
}

void
init_audialis_api(void)
{
	// core Audialis API functions
	register_api_function(g_duk, NULL, "LoadSound", js_LoadSound);

	// Sound object
	register_api_ctor(g_duk, "Sound", js_new_Sound, js_Sound_finalize);
	register_api_function(g_duk, "Sound", "toString", js_Sound_toString);
	register_api_prop(g_duk, "Sound", "length", js_Sound_get_length, NULL);
	register_api_prop(g_duk, "Sound", "pan", js_Sound_get_pan, js_Sound_set_pan);
	register_api_prop(g_duk, "Sound", "pitch", js_Sound_get_pitch, js_Sound_set_pitch);
	register_api_prop(g_duk, "Sound", "playing", js_Sound_get_playing, NULL);
	register_api_prop(g_duk, "Sound", "position", js_Sound_get_position, js_Sound_set_position);
	register_api_prop(g_duk, "Sound", "repeat", js_Sound_get_repeat, js_Sound_set_repeat);
	register_api_prop(g_duk, "Sound", "seekable", js_Sound_get_seekable, NULL);
	register_api_prop(g_duk, "Sound", "volume", js_Sound_get_volume, js_Sound_set_volume);
	register_api_function(g_duk, "Sound", "isPlaying", js_Sound_get_playing);
	register_api_function(g_duk, "Sound", "isSeekable", js_Sound_get_seekable);
	register_api_function(g_duk, "Sound", "getLength", js_Sound_get_length);
	register_api_function(g_duk, "Sound", "getPan", js_Sound_get_pan);
	register_api_function(g_duk, "Sound", "getPitch", js_Sound_get_pitch);
	register_api_function(g_duk, "Sound", "getPosition", js_Sound_get_position);
	register_api_function(g_duk, "Sound", "getRepeat", js_Sound_get_repeat);
	register_api_function(g_duk, "Sound", "getVolume", js_Sound_get_volume);
	register_api_function(g_duk, "Sound", "setPan", js_Sound_set_pan);
	register_api_function(g_duk, "Sound", "setPitch", js_Sound_set_pitch);
	register_api_function(g_duk, "Sound", "setPosition", js_Sound_set_position);
	register_api_function(g_duk, "Sound", "setRepeat", js_Sound_set_repeat);
	register_api_function(g_duk, "Sound", "setVolume", js_Sound_set_volume);
	register_api_function(g_duk, "Sound", "pause", js_Sound_pause);
	register_api_function(g_duk, "Sound", "play", js_Sound_play);
	register_api_function(g_duk, "Sound", "reset", js_Sound_reset);
	register_api_function(g_duk, "Sound", "stop", js_Sound_stop);
	
	// SoundStream object
	register_api_ctor(g_duk, "SoundStream", js_new_SoundStream, js_SoundStream_finalize);
	register_api_prop(g_duk, "SoundStream", "bufferSize", js_SoundStream_get_bufferSize, NULL);
	register_api_function(g_duk, "SoundStream", "buffer", js_SoundStream_buffer);
	register_api_function(g_duk, "SoundStream", "pause", js_SoundStream_pause);
	register_api_function(g_duk, "SoundStream", "play", js_SoundStream_play);
	register_api_function(g_duk, "SoundStream", "stop", js_SoundStream_stop);
}

static duk_ret_t
js_LoadSound(duk_context* ctx)
{
	duk_int_t n_args = duk_get_top(ctx);
	duk_require_string(ctx, 0);
	if (n_args >= 2) duk_require_boolean(ctx, 1);
	if (duk_safe_call(ctx, js_new_Sound, 0, 1) != 0)
		duk_throw(ctx);
	return 1;
}

static duk_ret_t
js_new_Sound(duk_context* ctx)
{
	duk_int_t n_args = duk_get_top(ctx);
	const char* filename = duk_require_string(ctx, 0);
	duk_bool_t is_stream = n_args >= 2 ? duk_require_boolean(ctx, 1) : true;

	sound_t* sound;

	sound = load_sound(filename, is_stream);
	if (sound == NULL)
		duk_error_ni(ctx, -1, DUK_ERR_ERROR, "Sound(): failed to load sound file '%s'", filename);
	duk_push_sphere_obj(ctx, "Sound", sound);
	return 1;
}

static duk_ret_t
js_Sound_finalize(duk_context* ctx)
{
	sound_t* sound;

	sound = duk_require_sphere_obj(ctx, 0, "Sound");
	free_sound(sound);
	return 0;
}

static duk_ret_t
js_Sound_toString(duk_context* ctx)
{
	duk_push_string(ctx, "[object sound]");
	return 1;
}

static duk_ret_t
js_Sound_get_length(duk_context* ctx)
{
	sound_t* sound;

	duk_push_this(ctx);
	sound = duk_require_sphere_obj(ctx, -1, "Sound");
	duk_pop(ctx);
	duk_push_number(ctx, get_sound_length(sound));
	return 1;
}

static duk_ret_t
js_Sound_get_pan(duk_context* ctx)
{
	sound_t* sound;

	duk_push_this(ctx);
	sound = duk_require_sphere_obj(ctx, -1, "Sound");
	duk_pop(ctx);
	duk_push_int(ctx, get_sound_pan(sound) * 255);
	return 1;
}

static duk_ret_t
js_Sound_set_pan(duk_context* ctx)
{
	int new_pan = duk_require_int(ctx, 0);

	sound_t* sound;

	duk_push_this(ctx);
	sound = duk_require_sphere_obj(ctx, -1, "Sound");
	duk_pop(ctx);
	set_sound_pan(sound, (float)new_pan / 255);
	return 0;
}

static duk_ret_t
js_Sound_get_pitch(duk_context* ctx)
{
	sound_t* sound;

	duk_push_this(ctx);
	sound = duk_require_sphere_obj(ctx, -1, "Sound");
	duk_pop(ctx);
	duk_push_number(ctx, get_sound_pitch(sound));
	return 1;
}

static duk_ret_t
js_Sound_set_pitch(duk_context* ctx)
{
	float new_pitch = duk_require_number(ctx, 0);

	sound_t* sound;

	duk_push_this(ctx);
	sound = duk_require_sphere_obj(ctx, -1, "Sound");
	duk_pop(ctx);
	set_sound_pitch(sound, new_pitch);
	return 0;
}

static duk_ret_t
js_Sound_get_playing(duk_context* ctx)
{
	sound_t* sound;

	duk_push_this(ctx);
	sound = duk_require_sphere_obj(ctx, -1, "Sound");
	duk_pop(ctx);
	duk_push_boolean(ctx, is_sound_playing(sound));
	return 1;
}

static duk_ret_t
js_Sound_get_position(duk_context* ctx)
{
	sound_t* sound;

	duk_push_this(ctx);
	sound = duk_require_sphere_obj(ctx, -1, "Sound");
	duk_pop(ctx);
	duk_push_number(ctx, get_sound_seek(sound));
	return 1;
}

static duk_ret_t
js_Sound_set_position(duk_context* ctx)
{
	long long new_pos = duk_require_number(ctx, 0);

	sound_t* sound;

	duk_push_this(ctx);
	sound = duk_require_sphere_obj(ctx, -1, "Sound");
	duk_pop(ctx);
	seek_sound(sound, new_pos);
	return 0;
}

static duk_ret_t
js_Sound_get_repeat(duk_context* ctx)
{
	sound_t* sound;

	duk_push_this(ctx);
	sound = duk_require_sphere_obj(ctx, -1, "Sound");
	duk_pop(ctx);
	duk_push_boolean(ctx, is_sound_looping(sound));
	return 1;
}

static duk_ret_t
js_Sound_set_repeat(duk_context* ctx)
{
	bool is_looped = duk_require_boolean(ctx, 0);

	sound_t* sound;

	duk_push_this(ctx);
	sound = duk_require_sphere_obj(ctx, -1, "Sound");
	duk_pop(ctx);
	set_sound_looping(sound, is_looped);
	return 0;
}

static duk_ret_t
js_Sound_get_seekable(duk_context* ctx)
{
	duk_push_true(ctx);
	return 1;
}

static duk_ret_t
js_Sound_get_volume(duk_context* ctx)
{
	sound_t* sound;

	duk_push_this(ctx);
	sound = duk_require_sphere_obj(ctx, -1, "Sound");
	duk_pop(ctx);
	duk_push_int(ctx, get_sound_gain(sound) * 255);
	return 1;
}

static duk_ret_t
js_Sound_set_volume(duk_context* ctx)
{
	float new_gain = duk_require_int(ctx, 0);

	sound_t* sound;

	duk_push_this(ctx);
	sound = duk_require_sphere_obj(ctx, -1, "Sound");
	duk_pop(ctx);
	set_sound_gain(sound, (float)new_gain / 255);
	return 0;
}

static duk_ret_t
js_Sound_pause(duk_context* ctx)
{
	sound_t* sound;

	duk_push_this(ctx);
	sound = duk_require_sphere_obj(ctx, -1, "Sound");
	duk_pop(ctx);
	stop_sound(sound, false);
	return 0;
}

static duk_ret_t
js_Sound_play(duk_context* ctx)
{
	int n_args = duk_get_top(ctx);

	bool     is_looping;
	sound_t* sound;

	duk_push_this(ctx);
	sound = duk_require_sphere_obj(ctx, -1, "Sound");
	duk_pop(ctx);
	if (n_args >= 1) {
		reload_sound(sound);
		is_looping = duk_require_boolean(ctx, 0);
		set_sound_looping(sound, is_looping);
	}
	play_sound(sound);
	return 0;
}

static duk_ret_t
js_Sound_reset(duk_context* ctx)
{
	sound_t* sound;

	duk_push_this(ctx);
	sound = duk_require_sphere_obj(ctx, -1, "Sound");
	duk_pop(ctx);
	seek_sound(sound, 0);
	return 0;
}

static duk_ret_t
js_Sound_stop(duk_context* ctx)
{
	sound_t* sound;

	duk_push_this(ctx);
	sound = duk_require_sphere_obj(ctx, -1, "Sound");
	duk_pop(ctx);
	stop_sound(sound, true);
	return 0;
}

static duk_ret_t
js_new_SoundStream(duk_context* ctx)
{
	int n_args = duk_get_top(ctx);
	int frequency = n_args >= 1 ? duk_require_int(ctx, 0) : 22050;

	stream_t* stream;

	if (!(stream = create_stream(frequency, 8)))
		duk_error_ni(ctx, -1, DUK_ERR_ERROR, "SoundStream(): Stream creation failed");
	duk_push_sphere_obj(ctx, "SoundStream", stream);
	return 1;
}

static duk_ret_t
js_SoundStream_finalize(duk_context* ctx)
{
	stream_t* stream;
	
	stream = duk_require_sphere_obj(ctx, 0, "SoundStream");
	free_stream(stream);
	return 0;
}

static duk_ret_t
js_SoundStream_get_bufferSize(duk_context* ctx)
{
	stream_t*    stream;

	duk_push_this(ctx);
	stream = duk_require_sphere_obj(ctx, -1, "SoundStream");
	duk_pop(ctx);
	duk_push_number(ctx, stream->feed_size);
	return 1;
}

static duk_ret_t
js_SoundStream_buffer(duk_context* ctx)
{
	bytearray_t* array = duk_require_sphere_bytearray(ctx, 0);

	const void* buffer;
	size_t      size;
	stream_t*   stream;

	duk_push_this(ctx);
	stream = duk_require_sphere_obj(ctx, -1, "SoundStream");
	duk_pop(ctx);
	buffer = get_bytearray_buffer(array);
	size = get_bytearray_size(array);
	feed_stream(stream, buffer, size);
	return 0;
}

static duk_ret_t
js_SoundStream_pause(duk_context* ctx)
{
	stream_t* stream;

	duk_push_this(ctx);
	stream = duk_require_sphere_obj(ctx, -1, "SoundStream");
	duk_pop(ctx);
	pause_stream(stream);
	return 0;
}

static duk_ret_t
js_SoundStream_play(duk_context* ctx)
{
	stream_t* stream;

	duk_push_this(ctx);
	stream = duk_require_sphere_obj(ctx, -1, "SoundStream");
	duk_pop(ctx);
	play_stream(stream);
	return 0;
}

static duk_ret_t
js_SoundStream_stop(duk_context* ctx)
{
	stream_t* stream;

	duk_push_this(ctx);
	stream = duk_require_sphere_obj(ctx, -1, "SoundStream");
	duk_pop(ctx);
	stop_stream(stream);
	return 0;
}