build:
	rm -f Main
	sleep 0.5
	g++ Main.cpp -o Main -lSDL2 -lrtaudio -O3
	./Main
