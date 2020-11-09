# redirectd
A simple web server that redirects requests for all clients to another site.

I built this as a bit of a toy but also as a dead-simple "web server" that could be used to e.g. bind to port 80 and simply redirect all traffic to HTTPS, rather than binding the "real" web server to port 80 and potentially inadvertently serving requests that should only be secure.

As of this moment, this is essentially a \*NIX-only daemon. You may be able to compile it on Windows with some support (e.g. cygwin, ming, etc.) but I make no promises.

It is currently only configured by recompiling. I plan to change that with future commits. I also plan to add privilege-dropping so you can "safely" run this as root to bind to lower privileged ports.

I have tested on Linux and MacOS.

## Compiling

```$ cc -std=c99 -Wall -pedantic -lm redirectd.c```

This works with gcc and clang.

## Using

```$ ./a.out```

This binds to port 8080 and returns a stock response to all clients. All network input is ignored.

You can test it while it's running like this:

$ nc localhost 8080
```HTTP/1.0 302 Found
Location: https://github.com/ChristopherSchultz/redirectd
Content-Length: 0
Connection: close

```

