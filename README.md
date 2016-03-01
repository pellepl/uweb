# uweb
Mini-kini web server aimed at embedded stuff, HTTP1.1.

MIT license.

Just the very basic stuff. Probably not fully compliant with the HTTP specs, but seems to work in most cases. Support for returning chunked or nonchunked data, reading POST requests, and handling multipart posts (e.g. uploading files). 

Nothing fancy.


```make all && make test``` to run tests.

```make server``` to open a uweb server on port 8080

More to come in a near future...
