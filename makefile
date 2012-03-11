naam = flexdir
daemon = flexdird
cli = flexdir
dirInstall = /usr/local/bin
dirConfig = /etc
dirInit = /etc/rc.d
dirLog = /var/log

main : $(daemon).o $(cli).o worker.o watcher.o checker.o
	g++ $(daemon).o worker.o watcher.o checker.o -lpthread -o $(daemon); g++ $(cli).o worker.o checker.o -lpthread -o $(cli)

$(daemon).o : $(daemon).cpp
	g++ -c $(daemon).cpp -lpthread

$(cli).o : $(cli).cpp
	g++ -c $(cli).cpp -lpthread

worker.o : worker.cpp
	g++ -c worker.cpp -lpthread

watcher.o: watcher.cpp
	g++ -c watcher.cpp 

checker.o: checker.cpp
	g++ -c checker.cpp 

clean:
	rm -rf *.o $(cli) $(daemon) $(cli).log

install: 
	install $(daemon) $(dirInstall)
	install $(cli) $(dirInstall)
	install $(cli).conf $(dirConfig)
	install initscript $(dirInit)/$(daemon)

uninstall: 
	rm $(dirInstall)/$(cli) $(dirInstall)/$(daemon) $(dirInit)/$(daemon) $(dirConfig)/$(cli).conf
    	
