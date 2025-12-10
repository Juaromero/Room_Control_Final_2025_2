from PIL import Image
import os

# Display size
WIDTH = 128
HEIGHT = 64
INPUT_IMAGE = "img.webp"  # change this if you use another file name
# Bits with value 1 will be drawn as "White" by ssd1306_DrawBitmap.
# Set PIXEL_ON_VALUE to 0 if you prefer black areas to be ON instead.
PIXEL_ON_VALUE = 255

# Open and convert to 1-bit black/white
img = Image.open(INPUT_IMAGE).convert("1")

# Resize if needed
img = img.resize((WIDTH, HEIGHT))

pixels = img.load()
bytes_out = []

# ssd1306_DrawBitmap expects bytes arranged horizontally: 8 pixels per row byte,
# with the MSB (bit 7) representing the leftmost pixel in that group.
row_byte_width = (WIDTH + 7) // 8
for y in range(HEIGHT):
    for byte_idx in range(row_byte_width):
        byte = 0
        for bit in range(8):
            x = byte_idx * 8 + bit
            if x >= WIDTH:
                continue
            if pixels[x, y] == PIXEL_ON_VALUE:
                byte |= (1 << (7 - bit))
        bytes_out.append(byte)

# Derive output header name from input image name
base_name = os.path.splitext(os.path.basename(INPUT_IMAGE))[0]
output_name = f"{base_name}_bitmap.h"

with open(output_name, "w") as f:
    f.write("#pragma once\n")
    f.write("#include <stdint.h>\n\n")
    f.write(f"#define LOGO_WIDTH  {WIDTH}\n")
    f.write(f"#define LOGO_HEIGHT {HEIGHT}\n\n")
    f.write("const uint8_t logo_bitmap[] = {\n")

    for i, b in enumerate(bytes_out):
        if i % 16 == 0:
            f.write("    ")
        f.write(f"0x{b:02X}, ")
        if i % 16 == 15:
            f.write("\n")
    if len(bytes_out) % 16 != 0:
        f.write("\n")
    f.write("};\n")

print(f"Wrote {output_name}")