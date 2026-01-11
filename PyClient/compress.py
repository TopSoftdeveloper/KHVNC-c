import ctypes
from ctypes import wintypes

# Load ntdll.dll
ntdll = ctypes.windll.ntdll

# Constants
COMPRESSION_FORMAT_LZNT1 = 2
COMPRESSION_ENGINE_STANDARD = 0

# Function prototypes
RtlGetCompressionWorkSpaceSize = ntdll.RtlGetCompressionWorkSpaceSize
RtlGetCompressionWorkSpaceSize.restype = wintypes.ULONG
RtlGetCompressionWorkSpaceSize.argtypes = [
    wintypes.USHORT,                # CompressionFormatAndEngine
    ctypes.POINTER(wintypes.ULONG), # CompressBufferWorkSpaceSize
    ctypes.POINTER(wintypes.ULONG)  # CompressFragmentWorkSpaceSize
]

# Function prototype
RtlCompressBuffer = ntdll.RtlCompressBuffer
RtlCompressBuffer.restype = wintypes.ULONG
RtlCompressBuffer.argtypes = [
    wintypes.USHORT,             # CompressionFormatAndEngine
    wintypes.LPVOID,             # UncompressedBuffer
    wintypes.ULONG,              # UncompressedBufferSize
    wintypes.LPVOID,             # CompressedBuffer
    wintypes.ULONG,              # CompressedBufferSize
    wintypes.ULONG,              # UncompressedChunkSize
    ctypes.POINTER(wintypes.ULONG),  # FinalCompressedSize
    wintypes.LPVOID              # WorkSpace
]

def compress(data: bytes) -> bytes:
    src_len = len(data)
    dst_len = src_len + (src_len // 16) + 0x1000  # safer buffer sizing
    chunk_size = 4096

    # Query the correct workspace size for LZNT1
    workspace_size = wintypes.ULONG()
    fragment_workspace_size = wintypes.ULONG()
    status = RtlGetCompressionWorkSpaceSize(
        COMPRESSION_FORMAT_LZNT1 | COMPRESSION_ENGINE_STANDARD,
        ctypes.byref(workspace_size),
        ctypes.byref(fragment_workspace_size)
    )
    if status != 0:
        raise RuntimeError(f"Failed to get workspace size: 0x{status:08X}")

    # Allocate buffers
    src_buf = ctypes.create_string_buffer(data, src_len)
    dst_buf = ctypes.create_string_buffer(dst_len)
    workspace = ctypes.create_string_buffer(workspace_size.value)
    final_size = wintypes.ULONG(0)

    # Perform compression
    status = RtlCompressBuffer(
        COMPRESSION_FORMAT_LZNT1 | COMPRESSION_ENGINE_STANDARD,
        ctypes.cast(src_buf, wintypes.LPVOID),
        src_len,
        ctypes.cast(dst_buf, wintypes.LPVOID),
        dst_len,
        chunk_size,
        ctypes.byref(final_size),
        ctypes.cast(workspace, wintypes.LPVOID)
    )

    if status != 0:
        raise RuntimeError(f"LZNT1 compression failed: 0x{status:08X}")

    return dst_buf.raw[:final_size.value]
