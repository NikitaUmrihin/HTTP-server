threadpool.c - 
	implementation of a thread pool : create threads and dispatch them (when possible) to some function .

server.c - 
	a basic server that can listen to http requests and reply accordingly ( send files or display errors ) .
	the server uses the threadpool , so it should also handle concurrent requests.
	usage command should be : " ./server <port> <number_of_threads> <max_requests> "
	when all values inside < > should be intergers