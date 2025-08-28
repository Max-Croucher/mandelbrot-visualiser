from http.server import HTTPServer, BaseHTTPRequestHandler
from PIL import Image

hostName = "localhost"
serverPort = 8080

MIN_X = -2.0
MIN_Y = -1.25
BASE_RANGE_X = 2.5
BASE_RANGE_Y = 2.5

def make_image(z: int, y: int, x: int) -> Image.Image:
    range_x: float = BASE_RANGE_X / (2 ** z)
    start_x: float = MIN_X + range_x * x
    range_y: float = BASE_RANGE_Y / (2 ** z)
    start_y: float = MIN_Y + range_y * y
    return Image.effect_mandelbrot((256,256), (start_x, start_y, start_x+range_x, start_y+range_y), 256)


class RequestHandler(BaseHTTPRequestHandler):

    def do_GET(self):
        print(self.path)

        if not self.path.startswith('/'):
            self.send_response(400)
            return
        if not self.path.endswith('.png'):
            self.send_response(400)
            return
        parts = self.path[1:-4].split('/')
        if len(parts) != 3:
            self.send_response(400)
            return
        try:
            z,x,y = map(int, parts)
        except ValueError:
            self.send_response(400)
            return
        
        self.send_response(200)
        self.send_header("Content-type", "data:image/png;base64")
        self.end_headers()
        make_image(z, y, x).save(self.wfile, format="png")

def main():
    server = HTTPServer((hostName, serverPort), RequestHandler)
    print("Server started http://%s:%s" % (hostName, serverPort))
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass

    server.server_close()
    print("Server stopped.")

if __name__ == "__main__":
    main()