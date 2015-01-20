COMPILER = g++ -std=c++11 -Wall -pedantic
FLAGS = -O3
#FLAGS = -O0 -g

all: main

main: libsolver.a main.o
	$(COMPILER) $(FLAGS) main.o -o bin/main -L. -lsolver

libsolver.a: result.o hamiltonian.o sa_solver.o 
	ar ruc libsolver.a result.o hamiltonian.o sa_solver.o
	ranlib libsolver.a

main.o: src/main.cpp src/result.hpp src/hamiltonian.hpp src/sa_solver.hpp
	$(COMPILER) $(FLAGS) -c src/main.cpp

sa_solver.o: src/sa_solver.hpp src/sa_solver.cpp
	$(COMPILER) $(FLAGS) -c src/sa_solver.cpp

result.o: src/result.hpp src/result.cpp
	$(COMPILER) $(FLAGS) -c src/result.cpp

hamiltonian.o: src/hamiltonian.hpp src/hamiltonian.cpp
	$(COMPILER) $(FLAGS) -c src/hamiltonian.cpp

clean:
	rm -f *.o *.a bin/main
