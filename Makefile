default:
	make quash

quash:
	gcc quash.c -o quash -std=gnu99

debug:
	gcc quash.c -o quash -std=gnu99 -g

clean:
	rm quash
