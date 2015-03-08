#include "image.h"

void init_surface_api (void);

extern void duk_push_sphere_surface (duk_context* ctx, image_t* image);

enum BLEND_MODE
{
	BLEND_BLEND,
	BLEND_REPLACE,
	BLEND_RGB_ONLY,
	BLEND_ALPHA_ONLY,
	BLEND_ADD,
	BLEND_SUBTRACT,
	BLEND_MULTIPLY,
	BLEND_AVERAGE,
	BLEND_INVERT
};
