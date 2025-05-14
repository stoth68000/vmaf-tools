
# Developed on ubuntu

INC=-g -I/usr/include/opencv4 -Wl,--copy-dt-needed-entries
LIB=-lopencv_core -lm -lopencv_highgui -lopencv_imgproc -lopencv_imgcodecs
BINS=pic2x2 picdiff picvmaf

all:	$(BINS)

pic2x2: pic2x2.c
	g++ $(INC) $(LIB) $@.c -o $@ $(LIB)

picdiff: picdiff.c
	g++ $(INC) $(LIB) $@.c -o $@ $(LIB)

picvmaf: picvmaf.c
	g++ $(INC) $(LIB) $@.c -o $@ $(LIB)

install:	all
	cp $(BINS) ../bin

clean:
	rm -f $(BINS)

build-devenv:
	docker build --network=host -t ubuntu-opencv-dev .

devenv:
	docker run --rm -it -v $(PWD)/../src:/src --network=host ubuntu-opencv-dev

run:
	docker run --rm -it -v $(PWD)/../src:/src --network=host ubuntu-opencv-dev /src/pic2x2
