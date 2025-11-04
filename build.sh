if [ ! -d "bin" ]; then
    mkdir bin
else
	rm bin/*
fi
g++ -g -O0 -I . -o bin/interrupts interrupts.cpp