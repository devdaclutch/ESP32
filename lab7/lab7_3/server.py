#!/usr/bin/env python3

from http.server import BaseHTTPRequestHandler, HTTPServer
import logging
import os
import json

def get_location():
    try:
        output = os.popen('curl -s http://ipinfo.io').read()
        data = json.loads(output)
        return data.get('city', 'Unknown')
    except Exception as e:
        logging.error(f"Error fetching location: {e}")
        return "Unknown"

class SimplePostHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/location":
            location = get_location()
            self.send_response(200)
            self.send_header('Content-type', 'text/plain')
            self.end_headers()
            self.wfile.write(location.encode('utf-8'))
        else:
            self.send_response(404)
            self.end_headers()

    def do_POST(self):
        content_length = int(self.headers.get('Content-Length', 0))
        post_data = self.rfile.read(content_length).decode('utf-8')

        try:
            data = json.loads(post_data)
            temp = data.get("temperature", "N/A")
            hum = data.get("humidity", "N/A")
            loc = data.get("location", "Unknown")
            outside = data.get("outside_temp", "N/A")

            logging.info(f"Data received:\nLocation: {loc}\nOutside Temp: {outside} °C\n"
                         f"Local Temp: {temp} °C\nHumidity: {hum} %")
        except Exception as e:
            logging.warning(f"Failed to parse JSON: {e}")
            logging.warning(f"Raw payload: {post_data}")

        self.send_response(200)
        self.send_header('Content-type', 'text/plain')
        self.end_headers()
        self.wfile.write(b"POST data received!")

def run(server_class=HTTPServer, handler_class=SimplePostHandler, port=1234):
    logging.basicConfig(level=logging.INFO)
    server_address = ('', port)
    httpd = server_class(server_address, handler_class)
    logging.info(f"Starting server on port {port}...")
    httpd.serve_forever()

if __name__ == "__main__":
    run()
