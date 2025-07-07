#!/usr/bin/env python3

from http.server import HTTPServer, BaseHTTPRequestHandler
import urllib.parse
import json

class SimpleHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header('Content-Type', 'text/html')
        self.end_headers()
        self.wfile.write(b"""
            <html>
                <body>
                    <h1>GET request received</h1>
                    <form method="post" action="/">
                        <input name="foo" value="bar">
                        <button type="submit">Send POST</button>
                    </form>
                </body>
            </html>
        """)

    def do_POST(self):
        length = int(self.headers.get('Content-Length', 0))
        body = self.rfile.read(length).decode('utf-8')

        print("Raw body:", body)  # for debugging

        try:
            data = json.loads(body)
            temp = data.get("temperature")
            hum = data.get("humidity")

            response = f"""
                <html>
                    <body>
                        <h1>Sensor Data Received</h1>
                        <p>Temperature: {temp} °C</p>
                        <p>Humidity: {hum} %</p>
                        <a href="/">Back</a>
                    </body>
                </html>
            """.encode('utf-8')

        except json.JSONDecodeError:
            response = f"""
                <html>
                    <body>
                        <h1>Invalid JSON</h1>
                        <pre>{body}</pre>
                        <a href="/">Back</a>
                    </body>
                </html>
            """.encode('utf-8')

        self.send_response(200)
        self.send_header('Content-Type', 'text/html')
        self.end_headers()
        self.wfile.write(response)


def run(host='', port=8000):
    server = HTTPServer((host, port), SimpleHandler)
    print(f"Serving on http://{host or 'localhost'}:{port}")
    server.serve_forever()


if __name__ == '__main__':
    run()
