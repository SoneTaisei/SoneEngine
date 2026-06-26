import math
from PIL import Image, ImageDraw

def create_star(size=112):
    image = Image.new("RGBA", (size, size), (255, 255, 255, 0))
    draw = ImageDraw.Draw(image)

    center = size / 2
    max_radius = size / 2
    min_radius = size / 6

    points = []
    for i in range(8):
        angle = i * math.pi / 4
        radius = max_radius if i % 2 == 0 else min_radius
        x = center + radius * math.cos(angle)
        y = center + radius * math.sin(angle)
        points.append((x, y))

    draw.polygon(points, fill=(255, 255, 0, 255))
    
    # Actually, let's make it look like the provided image: smoothed 4-pointed star, 
    # but polygon is sharp. To make it smooth/curved like a glint, we can draw a couple of ellipses or 
    # use a mathematical curve.
    return image

# Better star with curves
def create_smooth_star(size=256):
    image = Image.new("RGBA", (size, size), (255, 255, 255, 0))
    pixels = image.load()
    
    center = size / 2.0
    for y in range(size):
        for x in range(size):
            dx = abs(x - center) / center
            dy = abs(y - center) / center
            # Simple superellipse or similar formula for a 4-pointed star
            # like dx^p + dy^p = 1 with p < 1
            if dx == 0 and dy == 0:
                val = 1.0
            else:
                # To make it a star, dx^0.5 + dy^0.5 < 1
                val = math.pow(dx, 0.5) + math.pow(dy, 0.5)
            
            if val <= 1.0:
                # Inside the star
                # Add some anti-aliasing / soft edge
                dist = 1.0 - val
                alpha = min(255, int(dist * 255 * 5)) # scale up for sharp edge
                pixels[x, y] = (255, 255, 0, alpha)
                
    return image

img = create_smooth_star(128)
img.save("project/resources/Sprite/star.png")
