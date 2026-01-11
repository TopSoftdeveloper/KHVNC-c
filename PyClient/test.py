from compress import lznt1_compress

data = b"A" * 4096
compressed = lznt1_compress(data)
print(f"Compressed from {len(data)} to {len(compressed)} bytes")