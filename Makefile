obj-m := audio_test.o

audio_test-objs := xac97.o audio_buffer.o

all:
	make -C /homes/kk-449/Lab6/linux-2.6.35.7 M=$(PWD) modules
clean:
	make -C /homes/kk-449/Lab6/linux-2.6.35.7 M=$(PWD) clean
	rm -f xac97.o core
