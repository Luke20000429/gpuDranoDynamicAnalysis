all: avimod.o avilib.o

# compile supporting function file into object (binary)
avimod.o: avilib.h avimod.c 
	$(CXX) -c avimod.c

# compile supporting function file into object (binary)
avilib.o: avilib.h avilib.c
	$(CXX) -c avilib.c

# delete files
clean:
	rm *.o *.out
