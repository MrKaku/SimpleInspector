test : test.cpp simpleinspector.cpp simpleinspector.h
	g++ -pthread simpleinspector.cpp test.cpp -o test
