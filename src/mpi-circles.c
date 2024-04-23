/**
 * Michele Ceccacci
 * michele.ceccacci@studio.unibo.it
 * 0001027124
 */
#include "hpc.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <mpi/mpi.h>

typedef struct {
    float x, y;   /* coordinates of center */
} circle_t;

/* These constants can be replaced with #define's if necessary */
const float XMIN = 0.0;
const float XMAX = 1000.0;
const float YMIN = 0.0;
const float YMAX = 1000.0;
const float RMIN = 10.0;
const float RMAX = 100.0;
const float EPSILON = 1e-5;
const float K = 1.5;

int ncircles;
circle_t *circles = NULL;
float *circles_dx = NULL;
float *circles_dy = NULL;
float *circles_r = NULL;
float *recvbuf_dx = NULL;
float *recvbuf_dy = NULL;

/**
 * Return a random float in [a, b]
 */
float randab(float a, float b)
{
    return a + (((float)rand())/RAND_MAX) * (b-a);
}

/**
 * Set all displacements to zero.
 */
void reset_displacements( void )
{
    for (int i=0; i<ncircles; i++) {
        circles_dx[i] = circles_dy[i] = 0.0;
    }
}

/**
 * Create and populate the array `circles[]` with randomly placed
 * circls.
 *
 * Do NOT parallelize this function.
 */
void init_circles(int n)
{
    assert(circles == NULL);
    ncircles = n;
    circles = (circle_t*)malloc(n * sizeof(*circles));
    circles_dx = (float*)malloc(n * sizeof(float));
    circles_dy = (float*)malloc(n * sizeof(float));
    recvbuf_dx = malloc(n*sizeof(float));
    recvbuf_dy = malloc(n*sizeof(float));
    circles_r = malloc(n*sizeof(float));
    assert(recvbuf_dx != NULL);
    assert(recvbuf_dy != NULL);
    assert(circles != NULL);
    assert(circles_dx != NULL);
    assert(circles_dy != NULL);
    for (int i=0; i<n; i++) {
        circles[i].x = randab(XMIN, XMAX);
        circles[i].y = randab(YMIN, YMAX);
        circles_r[i] = randab(RMIN, RMAX);
    }
    reset_displacements();
}

int compute_forces_nth(int i) {
    int n_intersections = 0;
    for (int j=i+1; j < ncircles; j++) {
        const float deltax = circles[j].x - circles[i].x;
        const float deltay = circles[j].y - circles[i].y;
        /* hypotf(x,y) computes sqrtf(x*x + y*y) avoiding
            overflow. This function is defined in <math.h>, and
            should be available also on CUDA. In case of troubles,
            it is ok to use sqrtf(x*x + y*y) instead. */
        const float dist = hypotf(deltax, deltay);
        const float Rsum = circles_r[i] + circles_r[j];
        if (dist < Rsum - EPSILON) {
            n_intersections++;
            const float overlap = Rsum - dist;
            assert(overlap > 0.0);
            // avoid division by zero
            const float overlap_x = overlap / (dist + EPSILON) * deltax;
            const float overlap_y = overlap / (dist + EPSILON) * deltay;
            circles_dx[i] -= overlap_x / K;
            circles_dy[i] -= overlap_y / K;
            circles_dx[j] += overlap_x / K;
            circles_dy[j] += overlap_y / K;
        }
    }
    return n_intersections;
}

/**
 * Compute the force acting on each circle; returns the number of
 * overlapping pairs of circles (each overlapping pair must be counted
 * only once).
 */
int compute_forces( void )
{
    int my_rank, comm_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &comm_size);

    const int my_start = (ncircles+1)/2 * my_rank / comm_size;
    const int my_end = (ncircles+1)/2 * (my_rank+1) / comm_size;
    int n_intersections = 0;

    for (int i= my_start; i < my_end; i++) {
        n_intersections += compute_forces_nth(i);
        if (ncircles-i != i) {
            n_intersections += compute_forces_nth(ncircles-i);
        }
    }
    return n_intersections;
}

/**
 * Move the circles to a new position according to the forces acting
 * on each one.
 */
void move_circles( void )
{
    for (int i=0; i<ncircles; i++) {
        circles[i].x += recvbuf_dx[i];
        circles[i].y += recvbuf_dy[i];
    }
}

#ifdef MOVIE
/**
 * Dumps the circles into a text file that can be processed using
 * gnuplot. This function may be used for debugging purposes, or to
 * produce a movie of how the algorithm works.
 *
 * You may want to completely remove this function from the final
 * version.
 */
void dump_circles( int iterno )
{
    char fname[64];
    snprintf(fname, sizeof(fname), "circles-%05d.gp", iterno);
    FILE *out = fopen(fname, "w");
    const float WIDTH = XMAX - XMIN;
    const float HEIGHT = YMAX - YMIN;
    fprintf(out, "set term png notransparent large\n");
    fprintf(out, "set output \"circles-%05d.png\"\n", iterno);
    fprintf(out, "set xrange [%f:%f]\n", XMIN - WIDTH*.2, XMAX + WIDTH*.2 );
    fprintf(out, "set yrange [%f:%f]\n", YMIN - HEIGHT*.2, YMAX + HEIGHT*.2 );
    fprintf(out, "set size square\n");
    fprintf(out, "plot '-' with circles notitle\n");
    for (int i=0; i<ncircles; i++) {
        fprintf(out, "%f %f %f\n", circles[i].x, circles[i].y, circles[i].r);
    }
    fprintf(out, "e\n");
    fclose(out);
}
#endif

void assertNoErrors(int res) {
    assert(res == 0);
}

int main( int argc, char* argv[] )
{
    int n = 10000;
    int iterations = 20;

    MPI_Init(&argc, &argv);

    int my_rank, comm_size;
    MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Datatype MPI_CIRCLES;
    assertNoErrors(MPI_Type_contiguous(2, MPI_FLOAT, &MPI_CIRCLES));
    assertNoErrors(MPI_Type_commit(&MPI_CIRCLES));

    if ( argc > 3 ) {
        fprintf(stderr, "Usage: %s [ncircles [iterations]]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (argc > 1) {
        n = atoi(argv[1]);
    }

    if (argc > 2) {
        iterations = atoi(argv[2]);
    }
    double tstart_prog;
    init_circles(n);
    tstart_prog = hpc_gettime();
	#ifdef MOVIE
	    dump_circles(0);
	#endif
    MPI_Bcast(&ncircles, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&iterations, 1, MPI_INT, 0, MPI_COMM_WORLD);
    assertNoErrors(MPI_Bcast(circles_dx, ncircles, MPI_FLOAT, 0, MPI_COMM_WORLD));
    assertNoErrors(MPI_Bcast(circles_dy, ncircles, MPI_FLOAT, 0, MPI_COMM_WORLD));
    for (int it=0; it<iterations; it++) {
        int overlaps = 0;
        const double tstart_iter = hpc_gettime();
        reset_displacements();
        assertNoErrors(MPI_Bcast(circles, ncircles, MPI_CIRCLES, 0, MPI_COMM_WORLD));
        int n_overlaps = compute_forces();
        assertNoErrors(MPI_Reduce(&n_overlaps, &overlaps, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD));
        assertNoErrors(MPI_Reduce(circles_dx, recvbuf_dx, ncircles, MPI_FLOAT, MPI_SUM, 0, MPI_COMM_WORLD));
        assertNoErrors(MPI_Reduce(circles_dy, recvbuf_dy, ncircles, MPI_FLOAT, MPI_SUM, 0, MPI_COMM_WORLD));
        if (my_rank == 0) {
            move_circles();
            const double elapsed_iter = hpc_gettime() - tstart_iter;
            printf("Iteration %d of %d, %d overlaps (%f s)\n", it+1, iterations, overlaps, elapsed_iter);
            #ifdef MOVIE
                dump_circles(it+1);
            #endif
            };
    }

    if (my_rank == 0) {
        const double elapsed_prog = hpc_gettime() - tstart_prog;
        printf("Elapsed time: %f\n", elapsed_prog);
    }
    free(circles);
    free(recvbuf_dx);
    free(recvbuf_dy);
    free(circles_dx);
    free(circles_dy);
    MPI_Finalize();

    return EXIT_SUCCESS;
}
