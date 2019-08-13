import http.server
import socketserver
import random

PORT = 8000

sizes = 20
bodies = ["<body><p>{:s}</p></body></html>".format("d"*(100*2**i)).encode("ascii") for i in range(sizes)]

class MyHandler(http.server.BaseHTTPRequestHandler):
    def do_HEAD(self):
        self.send_response(200)
        self.send_header("Content-type", "text/html")
        self.end_headers()
    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-type", "text/html")
        self.end_headers()
        self.wfile.write(b"<html><head><title>Title goes here.</title></head>")
        self.wfile.write(random.choice(bodies))

try:
    server = http.server.HTTPServer(('localhost', PORT), MyHandler)
    server.server_bind = lambda self: None
    print('Started http server')
    server.serve_forever()
except KeyboardInterrupt:
    print('^C received, shutting down server')
    server.socket.close()
