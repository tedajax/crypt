#include "tx_types.h"

#include <GL/gl3w.h>

#define SOKOL_IMPL
#define SOKOL_GLCORE33
#define SOKOL_ASSERT
#include "sokol_gfx.h"

#define TX_MATH_IMPLEMENTATION
#include "tx_math.h"

#define TX_INPUT_IMPLEMENTATION
#include "tx_input.h"

#define TX_RAND_IMPLEMENTATION
#include "tx_rand.h"

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define JSMN_PARENT_LINKS
#include "jsmn.h"

#define HASH_IMPLEMENTATION
#include "hash.h"