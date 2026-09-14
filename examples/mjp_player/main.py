"""SD movie picker and streaming MJP1 player."""

import os
import struct
import time
import cyd


HEADER = "<4sHHHHIQ"
INDEX = "<QI"
PAGE_SIZE = 3
MAX_MOVIES = 30
INDEX_KEEPALIVE_INTERVAL = 32


def read_exact(source, size):
    data = source.read(size)
    if data is None or len(data) != size:
        raise OSError("truncated MJP file")
    return data


def find_movies(directory, depth, max_depth, movies):
    if depth > max_depth or len(movies) >= MAX_MOVIES:
        return movies
    try:
        entries = sorted(os.listdir(directory))
    except OSError:
        return movies
    for entry in entries:
        if not cyd.update():
            return movies
        if len(movies) >= MAX_MOVIES:
            break
        if entry.startswith("."):
            continue
        path = directory + "/" + entry if directory != "/" else "/" + entry
        try:
            mode = os.stat(path)[0]
        except OSError:
            continue
        if mode & 0x4000:
            find_movies(path, depth + 1, max_depth, movies)
        else:
            lower = entry.lower()
            if lower.endswith(".mjp") or lower.endswith(".mjpg"):
                movies.append(path)
    return movies


cyd.clear()
cyd.title("MJP MOVIE")
cyd.text(14, 72, "SCANNING SD FOR MOVIES...")
cyd.update()
try:
    os.mkdir("/sd/movies")
except OSError:
    pass
movies = []
find_movies("/sd", 0, 0, movies)
find_movies("/sd/movies", 0, 2, movies)
find_movies("/sd/apps/mjpplayer", 0, 2, movies)
movies = sorted(set(movies))
selected = None
page = 0


def choose(path):
    global selected
    selected = path


def choice_callback(path):
    def callback():
        choose(path)
    return callback


def next_page():
    global page
    page = (page + 1) % ((len(movies) + PAGE_SIZE - 1) // PAGE_SIZE)
    draw_picker()


def display_name(path):
    name = path.rsplit("/", 1)[-1]
    return name if len(name) <= 22 else name[:19] + "..."


def draw_picker():
    cyd.clear()
    cyd.title("MJP MOVIE")
    cyd.text(14, 38, "SELECT A MOVIE FROM SD")
    if not movies:
        cyd.text(14, 82, "NO .MJP OR .MJPG FILES")
        cyd.text(14, 102, "COPY MOVIES TO THE SD CARD")
        return
    start = page * PAGE_SIZE
    for row, path in enumerate(movies[start:start + PAGE_SIZE]):
        y = 60 + row * 40
        cyd.button("movie" + str(row), 12, y, 296, 32,
                   display_name(path), choice_callback(path))
    pages = (len(movies) + PAGE_SIZE - 1) // PAGE_SIZE
    cyd.text(14, 184, str(len(movies)) + " MOVIE(S)   PAGE " + str(page + 1) + "/" + str(pages))
    if pages > 1:
        cyd.button("next", 238, 176, 70, 28, "NEXT >", next_page)


draw_picker()
while selected is None and cyd.update():
    time.sleep_ms(20)

if selected is not None:
    with open(selected, "rb") as movie:
        magic, version, width, height, fps_x100, frame_count, index_offset = struct.unpack(
            HEADER, read_exact(movie, 24))
        if magic != b"MJP1" or version != 1:
            raise OSError("unsupported MJP format")
        if width != 240 or height != 240 or not fps_x100 or not frame_count:
            raise OSError("movie must be 240x240 MJP1")
        movie.seek(index_offset)
        frame_sizes = bytearray(frame_count * 2)
        first_jpeg_offset = 0
        expected_offset = 0
        largest_frame = 0
        for index in range(frame_count):
            # Large movies can have thousands of index entries. Keep the
            # cooperative runtime watchdog fed while scanning them.
            if index % INDEX_KEEPALIVE_INTERVAL == 0 and not cyd.update():
                raise OSError("movie loading cancelled")
            jpeg_offset, jpeg_size = struct.unpack(INDEX, read_exact(movie, 12))
            if jpeg_size < 4 or jpeg_size > 65535:
                raise OSError("invalid JPEG frame")
            if index == 0:
                first_jpeg_offset = jpeg_offset
            elif jpeg_offset != expected_offset:
                raise OSError("MJP frames must be contiguous")
            expected_offset = jpeg_offset + jpeg_size
            if jpeg_size > largest_frame:
                largest_frame = jpeg_size
            frame_sizes[index * 2] = jpeg_size & 255
            frame_sizes[index * 2 + 1] = jpeg_size >> 8
        # One buffer for every frame, allocated before playback starts. Reading
        # each frame into a new bytes object requests up to 65 kB per frame;
        # MicroPython answers by permanently growing its heap into the ESP-IDF
        # heap, which starves everything else for the rest of the boot.
        frame_buffer = bytearray(largest_frame)
        frame_view = memoryview(frame_buffer)
        if not cyd.video_begin():
            raise OSError("native video renderer unavailable")

        frame = 0
        jpeg_offset = first_jpeg_offset
        started = time.ticks_ms()
        while cyd.update():
            elapsed = max(0, time.ticks_diff(time.ticks_ms(), started))
            wanted = (elapsed * fps_x100) // 100000
            if wanted >= frame_count:
                frame = 0
                jpeg_offset = first_jpeg_offset
                started = time.ticks_ms()
                wanted = 0
            if wanted < frame:
                time.sleep_ms(1)
                continue
            while frame < wanted:
                size_index = frame * 2
                jpeg_offset += frame_sizes[size_index] | (frame_sizes[size_index + 1] << 8)
                frame += 1
            size_index = frame * 2
            jpeg_size = frame_sizes[size_index] | (frame_sizes[size_index + 1] << 8)
            movie.seek(jpeg_offset)
            frame_data = frame_view[:jpeg_size]
            if movie.readinto(frame_data) != jpeg_size:
                raise OSError("truncated MJP file")
            cyd.video_present(frame_data)
            jpeg_offset += jpeg_size
            frame += 1
