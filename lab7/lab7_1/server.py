from http.server import BaseHTTPRequestHandler, HTTPServer
import logging

class MyRequestHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        # Handle GET request
        self.send_response(200)
        self.send_header('Content-type', 'text/html')
        self.end_headers()
        self.wfile.write(b"Hello! You made a GET request.")

    def do_POST(self):
        # Handle POST request
        content_length = int(self.headers['Content-Length'])
        post_data = self.rfile.read(content_length)
        logging.info(f"Received POST data: {post_data.decode('utf-8')}")

        self.send_response(200)
        self.send_header('Content-type', 'text/html')
        self.end_headers()
        self.wfile.write(b"POST request received. Thank you!")

def run(server_class=HTTPServer, handler_class=MyRequestHandler, port=1234):
    logging.basicConfig(level=logging.INFO)
    server_address = ('', port)
    httpd = server_class(server_address, handler_class)
    logging.info(f"Starting server on port {port}...")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    httpd.server_close()
    logging.info("Server stopped.")

if __name__ == "__main__":
    run()
