# TODO: actually install this library.
from tasm._tasm import *
import ffmpeg
import cv2
import numpy as np

def numpy_array(self):
    return self.array().reshape(self.height(), self.width(), -1)[:,:,:3]

Image.numpy_array = numpy_array


class StreamInfo:
    def __init__(self, width, height, fps):
        self.width = width
        self.height = height
        self.fps = fps

def _get_video_stream_info(video_file: str) -> StreamInfo:
    probe = ffmpeg.probe(video_file)
    video_stream = next((stream for stream in
                         probe['streams'] if stream['codec_type'] == 'video'), None)
    return StreamInfo(int(video_stream['width']),
                      int(video_stream['height']),
                      eval(video_stream['avg_frame_rate']))

def _get_video_ff_byte_array(video_file: str, width, height, max_frame):
    out, _ = (
        ffmpeg
            .input(video_file)
            .filter_('select', 'lte(n, {})'.format(max_frame))
            .output('pipe:', format='rawvideo', pix_fmt='rgb24') # , vframes=95
            .run(capture_stdout=True)
    )
    video = (
        np.frombuffer(out, np.uint8).reshape([-1, height, width, 3])
    )
    return video

def get_video_roi(self, output_path, video_name, metadata_identifier, label, first_frame_inclusive, last_frame_exclusive):
    info = self.select_encoded(video_name, metadata_identifier, label, first_frame_inclusive, last_frame_exclusive)

    # TODO: don't hard code fps
    fps = 30
    out_width = info.max_object_width()
    if out_width % 2:
        out_width += 1
    out_height = info.max_object_height()
    if out_height % 2:
        out_height += 1
    vid_writer = cv2.VideoWriter(output_path, cv2.VideoWriter_fourcc(*'MP4V'), fps, (out_width, out_height))

    scan = info.scan()
    total_objects = 0
    while not scan.is_complete():
        next_info = scan.next()
        if next_info.is_empty():
            continue
        tile_and_rect_info = next_info.value()
        tile_info = tile_and_rect_info.tile_information()
        input_video = tile_info.filename
        frame_offset = tile_info.frame_offset
        max_frame_to_read = max(tile_info.frames_to_read) - frame_offset
        #         print(f'max_frame_to_read from {tile_info.filename}: ', max_frame_to_read)

        stream_info = _get_video_stream_info(input_video)
        frame_bytes = _get_video_ff_byte_array(input_video, stream_info.width, stream_info.height, max_frame_to_read)

        tile_rect = tile_info.tile_rect
        x_tile_offset = tile_rect.x
        y_tile_offset = tile_rect.y

        for rect in tile_and_rect_info.rectangles():
            if not rect.intersects(tile_rect):
                continue
            total_objects += 1
            current_frame_num = rect.id - frame_offset
            current_frame = frame_bytes[current_frame_num]

            x1 = max(0, rect.x - x_tile_offset)
            y1 = max(0, rect.y - y_tile_offset)
            x2 = x1 + rect.width
            y2 = y1 + rect.height
            pad_x = (out_width - rect.width) // 2
            pad_y = (out_height - rect.height) // 2

            roi_byte = current_frame[int(y1):int(y2), int(x1):int(x2), :]
            roi_byte = np.pad(roi_byte,
                              pad_width=[
                                  (pad_y, out_height - rect.height - pad_y),
                                  (pad_x, out_width - rect.width - pad_x),
                                  (0, 0)
                              ])
            frame = cv2.cvtColor(roi_byte, cv2.COLOR_RGB2BGR)
            vid_writer.write(frame)

    vid_writer.release()

TASM.get_video_roi = get_video_roi
