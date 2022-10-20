
all:
	make -C receiver/
	make -C hulkbuster/

	ln -s ${PWD}/hulkbuster/bin/hulkbuster ${PWD}/hulkbuster.o
	ln -s ${PWD}/receiver/bin/receiver ${PWD}/receiver.o

clean:
	make clean -C receiver/
	make clean -C hulkbuster/

	rm ${PWD}/hulkbuster.o
	rm ${PWD}/receiver.o