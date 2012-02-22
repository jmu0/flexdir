naam = flexdir
main = testWorker

flexdir: $(main).o worker.o watcher.o checker.o
	g++ $(main).o worker.o watcher.o checker.o -lpthread -o $(naam)

testWorker.o: testWorker.cpp
	g++ -c testWorker.cpp -lpthread

main.o : main.cpp
	g++ -c main.cpp -lpthread
	
worker.o : worker.cpp
	g++ -c worker.cpp -lpthread

watcher.o: watcher.cpp
	g++ -c watcher.cpp 

checker.o: checker.cpp
	g++ -c checker.cpp 

clean:
	rm -rf *.o $(naam) $(naam).log

install: 
	install $(naam) /usr/local/bin
	install initscript /etc/rc.d/$(naam)

uninstall: 
	rm /usr/local/bin/$(naam) /etc/rc.d/$(naam) 
    	
