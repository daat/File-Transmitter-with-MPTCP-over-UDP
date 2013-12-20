all: agent transmitter
agent: agent.c
	gcc agent.c -o agent
transmitter: transmitter.c
	gcc transmitter.c -o transmitter
clean:
	rm -f agent transmitter
