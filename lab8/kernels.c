/********************************************************
 * Kernels to be optimized for the CS:APP Performance Lab
 ********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "defs.h"

/* 
 * Please fill in the following team struct 
 */
team_t team = {
    "5120379064",         /* Student ID */

    "丁卓成",     	      /* Your Name */
    "569375794@qq.com",   /* First member email address */

    "",                   /* Second member full name (leave blank if none) */
    ""                    /* Second member email addr (leave blank if none) */
};

/***************
 * ROTATE KERNEL
 ***************/

/******************************************************
 * Your different versions of the rotate kernel go here
 ******************************************************/
/*
 *Add the description of your Rotate implementation here!!!
 *1. Brief Intro of method
 *2. CPE Achieved
 *3. other words
 */

/* 
 * naive_rotate - The naive baseline version of rotate 
 */
char naive_rotate_descr[] = "naive_rotate: Naive baseline implementation";
void naive_rotate(int dim, pixel *src, pixel *dst) 
{
    int i, j;

    for (i = 0; i < dim; i++)
	for (j = 0; j < dim; j++)
	    dst[RIDX(dim-1-j, i, dim)] = src[RIDX(i, j, dim)];
}

/* 
 * rotate - Your current working version of rotate
 * IMPORTANT: This is the version you will be graded on
 */

#define move(a) dst[RIDX(dim-1-j, i+(a), dim)] = src[RIDX(i+(a), j, dim)];

char rotate_descr[] = "rotate: Current working version";
void rotate(int dim, pixel *src, pixel *dst) 
{

    int i, j;

    for (i = 0; i < dim; i += 16) 
        for (j = 0; j < dim; j++) {
            move(0);move(1);move(2);move(3);
            move(4);move(5);move(6);move(7);
            move(8);move(9);move(10);move(11);
            move(12);move(13);move(14);move(15);
        }

}

/*********************************************************************
 * register_rotate_functions - Register all of your different versions
 *     of the rotate kernel with the driver by calling the
 *     add_rotate_function() for each test function. When you run the
 *     driver program, it will test and report the performance of each
 *     registered test function.  
 *********************************************************************/

void register_rotate_functions() 
{
    add_rotate_function(&naive_rotate, naive_rotate_descr);   
    add_rotate_function(&rotate, rotate_descr);   
    /* ... Register additional test functions here */
}


/***************
 * SMOOTH KERNEL
 **************/
/*
 *Add description of your Smooth Implementation here!!!
 *1. Brief Intro of your method
 *2. CPE Achieved
 *3. Any other...
 */
/***************************************************************
 * Various typedefs and helper functions for the smooth function
 * You may modify these any way you like.
 **************************************************************/

/* A struct used to compute averaged pixel value */
typedef struct {
    int red;
    int green;
    int blue;
    int num;
} pixel_sum;

/* Compute min and max of two integers, respectively */
static int min(int a, int b) { return (a < b ? a : b); }
static int max(int a, int b) { return (a > b ? a : b); }

/* 
 * initialize_pixel_sum - Initializes all fields of sum to 0 
 */
static void initialize_pixel_sum(pixel_sum *sum) 
{
    sum->red = sum->green = sum->blue = 0;
    sum->num = 0;
    return;
}

/* 
 * accumulate_sum - Accumulates field values of p in corresponding 
 * fields of sum 
 */
static void accumulate_sum(pixel_sum *sum, pixel p) 
{
    sum->red += (int) p.red;
    sum->green += (int) p.green;
    sum->blue += (int) p.blue;
    sum->num++;
    return;
}

/* 
 * assign_sum_to_pixel - Computes averaged pixel value in current_pixel 
 */
static void assign_sum_to_pixel(pixel *current_pixel, pixel_sum sum) 
{
    current_pixel->red = (unsigned short) (sum.red/sum.num);
    current_pixel->green = (unsigned short) (sum.green/sum.num);
    current_pixel->blue = (unsigned short) (sum.blue/sum.num);
    return;
}

/* 
 * avg - Returns averaged pixel value at (i,j) 
 */
static pixel avg(int dim, int i, int j, pixel *src) 
{
    int ii, jj;
    pixel_sum sum;
    pixel current_pixel;

    initialize_pixel_sum(&sum);
    for(ii = max(i-1, 0); ii <= min(i+1, dim-1); ii++) 
	for(jj = max(j-1, 0); jj <= min(j+1, dim-1); jj++) 
	    accumulate_sum(&sum, src[RIDX(ii, jj, dim)]);

    assign_sum_to_pixel(&current_pixel, sum);
    return current_pixel;
}

/******************************************************
 * Your different versions of the smooth kernel go here
 ******************************************************/

/*
 * naive_smooth - The naive baseline version of smooth 
 */
char naive_smooth_descr[] = "naive_smooth: Naive baseline implementation";
void naive_smooth(int dim, pixel *src, pixel *dst) 
{
    int i, j;

    for (i = 0; i < dim; i++)
	for (j = 0; j < dim; j++)
	    dst[RIDX(i, j, dim)] = avg(dim, i, j, src);
}

/*
 * smooth - Your current working version of smooth. 
 * IMPORTANT: This is the version you will be graded on
 */
char smooth_descr[] = "smooth: Current working version";
void smooth(int dim, pixel *src, pixel *dst) 
{
    int red[dim*dim];
    int green[dim*dim];
    int blue[dim*dim];

    int accum_red, accum_green, accum_blue; 

    int i, j;

    for (i = 0; i < dim; i++) {
        accum_red = (int)src[RIDX(i, 0, dim)].red + (int)src[RIDX(i, 1, dim)].red;
        accum_green = (int)src[RIDX(i, 0, dim)].green + (int)src[RIDX(i, 1, dim)].green;
        accum_blue = (int)src[RIDX(i, 0, dim)].blue + (int)src[RIDX(i, 1, dim)].blue;
        red[RIDX(i, 0, dim)] = accum_red;
        green[RIDX(i, 0, dim)] = accum_green;
        blue[RIDX(i, 0, dim)] = accum_blue;

        for (j = 2; j < dim; j++) {
            accum_red += (int)src[RIDX(i, j, dim)].red;
            accum_green += (int)src[RIDX(i, j, dim)].green;
            accum_blue += (int)src[RIDX(i, j, dim)].blue;
            red[RIDX(i, j-1, dim)] = accum_red;
            green[RIDX(i, j-1, dim)] = accum_green;
            blue[RIDX(i, j-1, dim)] = accum_blue;
            accum_red -= (int)src[RIDX(i, j-2, dim)].red;
            accum_green -= (int)src[RIDX(i, j-2, dim)].green;
            accum_blue -= (int)src[RIDX(i, j-2, dim)].blue;
        }

        red[RIDX(i, dim-1, dim)] = accum_red;
        green[RIDX(i, dim-1, dim)] = accum_green;
        blue[RIDX(i, dim-1, dim)] = accum_blue;
    }

    //-------------------------------------------------------------
    
    dst[RIDX(0, 0, dim)].red = (unsigned short)( (red[RIDX(0, 0, dim)] + red[RIDX(1, 0, dim)]) / 4);
    dst[RIDX(0, 0, dim)].green = (unsigned short)( (green[RIDX(0, 0, dim)] + green[RIDX(1, 0, dim)]) / 4);
    dst[RIDX(0, 0, dim)].blue = (unsigned short)( (blue[RIDX(0, 0, dim)] + blue[RIDX(1, 0, dim)]) / 4);
    for (j = 1; j < dim-1; j++) {
        dst[RIDX(0, j, dim)].red = (unsigned short)( (red[RIDX(0, j, dim)] + red[RIDX(1, j, dim)]) / 6);
        dst[RIDX(0, j, dim)].green = (unsigned short)( (green[RIDX(0, j, dim)] + green[RIDX(1, j, dim)]) / 6);
        dst[RIDX(0, j, dim)].blue = (unsigned short)( (blue[RIDX(0, j, dim)] + blue[RIDX(1, j, dim)]) / 6);
    }
    dst[RIDX(0, dim-1, dim)].red = (unsigned short)( (red[RIDX(0, dim-1, dim)] + red[RIDX(1, dim-1, dim)]) / 4);
    dst[RIDX(0, dim-1, dim)].green = (unsigned short)( (green[RIDX(0, dim-1, dim)] + green[RIDX(1, dim-1, dim)]) / 4);
    dst[RIDX(0, dim-1, dim)].blue = (unsigned short)( (blue[RIDX(0, dim-1, dim)] + blue[RIDX(1, dim-1, dim)]) / 4);

    //-------------------------------------------------------------

    for (i = 1; i < dim-1; i++) {
        dst[RIDX(i, 0, dim)].red = (unsigned short)( (red[RIDX(i-1, 0, dim)] +
                                    red[RIDX(i, 0, dim)] + red[RIDX(i+1, 0, dim)]) / 6);
        dst[RIDX(i, 0, dim)].green = (unsigned short)( (green[RIDX(i-1, 0, dim)] +
                                    green[RIDX(i, 0, dim)] + green[RIDX(i+1, 0, dim)]) / 6);
        dst[RIDX(i, 0, dim)].blue = (unsigned short)( (blue[RIDX(i-1, 0, dim)] +
                                    blue[RIDX(i, 0, dim)] + blue[RIDX(i+1, 0, dim)]) / 6);
        for (j = 1; j < dim-1; j++) {
            dst[RIDX(i, j, dim)].red = (unsigned short)( (red[RIDX(i-1, j, dim)] +
                                        red[RIDX(i, j, dim)] + red[RIDX(i+1, j, dim)]) / 9);
            dst[RIDX(i, j, dim)].green = (unsigned short)( (green[RIDX(i-1, j, dim)] +
                                        green[RIDX(i, j, dim)] + green[RIDX(i+1, j, dim)]) / 9);
            dst[RIDX(i, j, dim)].blue = (unsigned short)( (blue[RIDX(i-1, j, dim)] +
                                        blue[RIDX(i, j, dim)] + blue[RIDX(i+1, j, dim)]) / 9);
        }
        dst[RIDX(i, dim-1, dim)].red = (unsigned short)( (red[RIDX(i-1, dim-1, dim)] +
                                    red[RIDX(i, dim-1, dim)] + red[RIDX(i+1, dim-1, dim)]) / 6);
        dst[RIDX(i, dim-1, dim)].green = (unsigned short)( (green[RIDX(i-1, dim-1, dim)] +
                                    green[RIDX(i, dim-1, dim)] + green[RIDX(i+1, dim-1, dim)]) / 6);
        dst[RIDX(i, dim-1, dim)].blue = (unsigned short)( (blue[RIDX(i-1, dim-1, dim)] +
                                    blue[RIDX(i, dim-1, dim)] + blue[RIDX(i+1, dim-1, dim)]) / 6);
    }

    //-------------------------------------------------------------

    dst[RIDX(dim-1, 0, dim)].red = (unsigned short)( (red[RIDX(dim-2, 0, dim)] + red[RIDX(dim-1, 0, dim)]) / 4);
    dst[RIDX(dim-1, 0, dim)].green = (unsigned short)( (green[RIDX(dim-2, 0, dim)] + green[RIDX(dim-1, 0, dim)]) / 4);
    dst[RIDX(dim-1, 0, dim)].blue = (unsigned short)( (blue[RIDX(dim-2, 0, dim)] + blue[RIDX(dim-1, 0, dim)]) / 4);
    for (j = 1; j < dim-1; j++) {
        dst[RIDX(dim-1, j, dim)].red = (unsigned short)( (red[RIDX(dim-2, j, dim)] + red[RIDX(dim-1, j, dim)]) / 6);
        dst[RIDX(dim-1, j, dim)].green = (unsigned short)( (green[RIDX(dim-2, j, dim)] + green[RIDX(dim-1, j, dim)]) / 6);
        dst[RIDX(dim-1, j, dim)].blue = (unsigned short)( (blue[RIDX(dim-2, j, dim)] + blue[RIDX(dim-1, j, dim)]) / 6);
    }
    dst[RIDX(dim-1, dim-1, dim)].red = (unsigned short)( (red[RIDX(dim-2, dim-1, dim)] + red[RIDX(dim-1, dim-1, dim)]) / 4);
    dst[RIDX(dim-1, dim-1, dim)].green = (unsigned short)( (green[RIDX(dim-2, dim-1, dim)] + green[RIDX(dim-1, dim-1, dim)]) / 4);
    dst[RIDX(dim-1, dim-1, dim)].blue = (unsigned short)( (blue[RIDX(dim-2, dim-1, dim)] + blue[RIDX(dim-1, dim-1, dim)]) / 4);
}


/********************************************************************* 
 * register_smooth_functions - Register all of your different versions
 *     of the smooth kernel with the driver by calling the
 *     add_smooth_function() for each test function.  When you run the
 *     driver program, it will test and report the performance of each
 *     registered test function.  
 *********************************************************************/

void register_smooth_functions() {
    add_smooth_function(&smooth, smooth_descr);
    add_smooth_function(&naive_smooth, naive_smooth_descr);
    /* ... Register additional test functions here */
}
