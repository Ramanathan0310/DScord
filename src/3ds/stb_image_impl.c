// stb_image implementation, kept in its own file so its warnings stay out of
// the rest of the build.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_GIF
#define STBI_ONLY_BMP
#define STBI_NO_STDIO
#include "stb_image.h"
