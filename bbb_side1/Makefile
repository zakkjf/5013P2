CFLAGS= -Wall -Werror -g
LDFLAGS=-pthread -lm
CC=gcc
#arm-linux-gnueabihf-gcc
OBJECTS=main.o uart_driver.o parser.o gps/gps_calc.o gps/gps_decoder.o
TARGET=5013P2

all: $(TARGET)
	
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

clean:
	rm ./$(TARGET) *.o
	rm ./gps/*.o
