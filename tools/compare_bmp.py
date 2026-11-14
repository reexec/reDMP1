#!/usr/bin/env python3
"""Compare two uncompressed 24bpp/32bpp BMP files using Python stdlib only."""

import argparse
import struct
import sys


def read_bmp(path):
    with open(path, "rb") as f:
        data = f.read()
    if len(data) < 54 or data[:2] != b"BM":
        raise ValueError(f"Not a valid BMP file: {path}")
    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    bpp = struct.unpack_from("<H", data, 28)[0]
    comp = struct.unpack_from("<I", data, 30)[0]
    if comp != 0 or bpp not in (24, 32):
        raise ValueError(f"Unsupported BMP format (bpp={bpp}, comp={comp}): {path}")
    top_down = height < 0
    h = abs(height)
    w = width
    bytes_pp = bpp // 8
    stride = ((w * bytes_pp + 3) // 4) * 4

    rows = []
    for y in range(h):
        src_y = y if top_down else (h - 1 - y)
        row_start = pixel_offset + src_y * stride
        row = bytearray(w * 3)
        for x in range(w):
            p = row_start + x * bytes_pp
            b, g, r = data[p], data[p + 1], data[p + 2]
            row[x * 3 + 0] = r
            row[x * 3 + 1] = g
            row[x * 3 + 2] = b
        rows.append(row)
    return w, h, rows


def write_bmp_rgb(path, w, h, rows):
    stride = ((w * 3 + 3) // 4) * 4
    img_size = stride * h
    header = struct.pack(
        "<2sIHHI", b"BM", 54 + img_size, 0, 0, 54
    ) + struct.pack(
        "<IiiHHIIiiII", 40, w, h, 1, 24, 0, img_size, 2835, 2835, 0, 0
    )
    out = bytearray(header)
    pad = b"\x00" * (stride - w * 3)
    for y in range(h - 1, -1, -1):
        row = rows[y]
        for x in range(w):
            out.append(row[x * 3 + 2])
            out.append(row[x * 3 + 1])
            out.append(row[x * 3 + 0])
        out.extend(pad)
    with open(path, "wb") as f:
        f.write(out)


def compare_bmps(path_a, path_b, tol=1, diff_out=None):
    wa, ha, rows_a = read_bmp(path_a)
    wb, hb, rows_b = read_bmp(path_b)
    if (wa, ha) != (wb, hb):
        raise ValueError(f"Dimension mismatch: {wa}x{ha} vs {wb}x{hb}")

    max_diff = 0
    exceed_count = 0
    min_x, min_y = wa, ha
    max_x, max_y = -1, -1
    diff_rows = [bytearray(wa * 3) for _ in range(ha)] if diff_out else None

    for y in range(ha):
        ra = rows_a[y]
        rb = rows_b[y]
        for x in range(wa):
            i = x * 3
            dr = abs(ra[i + 0] - rb[i + 0])
            dg = abs(ra[i + 1] - rb[i + 1])
            db = abs(ra[i + 2] - rb[i + 2])
            px_max = max(dr, dg, db)
            if px_max > max_diff:
                max_diff = px_max
            if px_max > tol:
                exceed_count += 1
                if x < min_x:
                    min_x = x
                if y < min_y:
                    min_y = y
                if x > max_x:
                    max_x = x
                if y > max_y:
                    max_y = y
                if diff_rows is not None:
                    diff_rows[y][i + 0] = min(255, dr * 4 + 64)
                    diff_rows[y][i + 1] = min(255, dg * 4)
                    diff_rows[y][i + 2] = min(255, db * 4 + 64)
            elif diff_rows is not None:
                diff_rows[y][i + 0] = ra[i + 0] // 4
                diff_rows[y][i + 1] = ra[i + 1] // 4
                diff_rows[y][i + 2] = ra[i + 2] // 4

    if diff_out and diff_rows is not None:
        write_bmp_rgb(diff_out, wa, ha, diff_rows)

    return {
        "width": wa,
        "height": ha,
        "max_diff": max_diff,
        "exceed_count": exceed_count,
        "bbox": (min_x, min_y, max_x, max_y) if exceed_count > 0 else None,
    }


def main():
    ap = argparse.ArgumentParser(description="Compare two BMP images pixel-by-pixel.")
    ap.add_argument("bmp_a")
    ap.add_argument("bmp_b")
    ap.add_argument("--tol", type=int, default=1, help="Per-channel tolerance (default: 1)")
    ap.add_argument("--diff-out", help="Optional output BMP highlighting differing pixels")
    args = ap.parse_args()

    res = compare_bmps(args.bmp_a, args.bmp_b, tol=args.tol, diff_out=args.diff_out)
    bbox_str = (
        f"({res['bbox'][0]},{res['bbox'][1]})..({res['bbox'][2]},{res['bbox'][3]})"
        if res["bbox"]
        else "none"
    )
    print(
        f"{res['width']}x{res['height']} max_diff={res['max_diff']} "
        f"exceed(>{args.tol})={res['exceed_count']} bbox={bbox_str}"
    )
    return 0 if res["exceed_count"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
