#!/bin/bash
# (See https://arc-ts.umich.edu/greatlakes/user-guide/ for command details)

# Set up batch job settings
#SBATCH --job-name=mpi_hello_world
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=32
#SBATCH --mem-per-cpu=1g
#SBATCH --time=00:05:00
#SBATCH --account=eecs587f23_class
#SBATCH --partition=standard


# Run your program

# (">" redirects the print output of your program, in this case to "output.txt")

# -np command sets number of processors to run program with. If you specify more processors
# using -np command than you have requisitioned using the #SBATCH --ntasks-per-node command
# your program will not run

# ./hello_world is the name of the program executable

# --bind-to core sets up mpi environment and specifies which hardware to bind MPI processes to
# you don't need to mess with this
module purge
module load gcc
module load openmpi
g++ -c Individual.cpp -o Individual.o -I ../../boost-ver/include/
g++ -c GraphHandler.cpp -o GraphHandler.o -I ../../boost-ver/include/
g++ -fopenmp Infect.cpp Individual.o GraphHandler.o -o 3 -I ../../boost-ver/include/

./3 > output3.txt
