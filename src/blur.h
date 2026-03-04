/* blur.h - Kawase dual blur pipeline */
#ifndef BLUR_H
#define BLUR_H

#include <glad/glad.h>
#include <stdbool.h>

#define BLUR_PASSES 4

typedef struct BlurPipeline BlurPipeline;

/* Create blur pipeline for given screen dimensions */
BlurPipeline *blur_create(int width, int height);
void blur_destroy(BlurPipeline *bp);

/* Resize all FBOs */
void blur_resize(BlurPipeline *bp, int width, int height);

/* Run the blur: takes a source texture, outputs blurred texture */
GLuint blur_apply(BlurPipeline *bp, GLuint source_tex, int width, int height);

/* Get the blurred result texture */
GLuint blur_get_result(const BlurPipeline *bp);

#endif /* BLUR_H */
