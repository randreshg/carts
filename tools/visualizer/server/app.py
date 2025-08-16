from http.server import SimpleHTTPRequestHandler
from socketserver import TCPServer
import os
import argparse


def main():
    parser = argparse.ArgumentParser(description="Serve ARTS visualizer UI")
    parser.add_argument("--port", type=int, default=8088)
    parser.add_argument("--dir", type=str, default=os.path.dirname(__file__))
    args = parser.parse_args()

    os.chdir(args.dir)
    handler = SimpleHTTPRequestHandler
    with TCPServer(("0.0.0.0", args.port), handler) as httpd:
        print(f"Serving ARTS visualizer UI on http://localhost:{args.port}")
        httpd.serve_forever()


if __name__ == "__main__":
    main()


