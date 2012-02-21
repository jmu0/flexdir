naam = flexdir
daemon = flexdird
cli = flexdir

$(daemon): $(daemon).o worker.o watcher.o checker.o
	g++ $(daemon).o worker.o watcher.o checker.o -lpthread -o $(daemon)

$(cli) : $(cli).o worker.o checker.o
	g++ $(cli).o worker.o checker.o -o $(cli)

$(daemon).o : $(daemon).cpp
	g++ -c flexdird.cpp -lpthread

$(cli).o : $(cli).cpp
	g++ -c flexdir.cpp

worker.o : worker.cpp
	g++ -c worker.cpp -lpthread

watcher.o: watcher.cpp
	g++ -c watcher.cpp 

checker.o: checker.cpp
	g++ -c checker.cpp 

clean:
	rm -rf *.o $(cli) $(daemon) $(cli).log

install: 
	install $(daemon) /usr/local/bin
	install $(cli) /usr/local/bin
	install initscript /etc/rc.d/$(daemon)

uninstall: 
	rm /usr/local/bin/$(cli) /usr/local/bin/$(daemon) /etc/rc.d/$(daemon)
    	
