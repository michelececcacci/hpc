# Default makefile for the circles intersections program.
#
# Available targets:
#
# - make
#   builds the standard executable
#
# - make clean
#   remove all output files and executables
#
# - make movie
#   compile the executable circles.movie that writes a gnuplot
#   file at each iteration; the executable is run and the output
#   is processed to produce an animation circles.avi

# Last modified 2023-12-06 by Moreno Marzolla

EXE:=circles
OMP_EXE:=omp-$(EXE)
MPI_EXE:=mpi-$(EXE)
CFLAGS=-std=c99 -Wall -Wpedantic
LDLIBS+=-lm

ALL: $(EXE)

$(EXE).movie: CFLAGS+=-DMOVIE
$(EXE).movie: $(EXE).c
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

$(OMP_EXE): $(OMP_EXE).c
	$(CC) $(OMP_EXE).c -fopenmp $(CFLAGS) $(LDLIBS) -o $(OMP_EXE)

$(MPI_EXE): $(MPI_EXE).c
	mpicc $(MPI_EXE).c $(CFLAGS) $(LDLIBS) -o $(MPI_EXE)

movie: $(EXE).movie
	./$(EXE).movie 200 500
	for f in *.gp; do echo "Processing $$f"; gnuplot "$$f"; done
	ffmpeg -y -i "circles-%05d.png" -vcodec mpeg4 circles.avi

clean:
	\rm -f $(EXE) $(EXE).movie *.o *~ *.gp *.png *.avi $(OMP_EXE)
