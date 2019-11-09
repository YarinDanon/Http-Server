# http-server-c

implementation of http server in c in order to enable multithreaded program the server should create threads that handle the connection 
with the client every time the server need thread it take one from the pool or enqueue the request if there is no availeble thread in the
pool

# usage

./server < port > < pool-size > < max-number-of-request >

- port is the port number the server will listen on
- pool size is the number of threads in the pool
- number of request is the maximum number of request the server will handle before it destroys the pool
