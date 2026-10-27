#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成原版布局所需的全部工具栏图标 (PNG 横条 16x16*N)"""
import struct, zlib

W = H = 16
def chunk(t, d):
    c = struct.pack('>I', len(d)) + t + d
    return c + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)

FG = (20, 40, 90, 255); BG = (0, 0, 0, 0); HL = (58, 144, 219, 255)
def blank(): return [[BG]*W for _ in range(H)]
def px(im, pts, c=FG):
    for x, y in pts:
        if 0 <= x < W and 0 <= y < H: im[y][x] = c
def ro(im, x0, y0, x1, y1, c=FG):
    for x in range(x0, x1+1): px(im, [(x, y0), (x, y1)], c)
    for y in range(y0, y1+1): px(im, [(x0, y), (x1, y)], c)
def fr(im, x0, y0, x1, y1, c=FG):
    for y in range(y0, y1+1):
        for x in range(x0, x1+1): px(im, [(x, y)], c)
def mirror(im): return [[im[y][W-1-x] for x in range(W)] for y in range(H)]

def i_new():
    im=blank(); ro(im,3,1,12,14); fr(im,4,2,11,13,BG)
    px(im,[(10,2),(11,3),(9,3),(12,2)],FG)
    for x in range(5,11): px(im,[(x,4),(x,6),(x,8)],HL)
    return im
def i_open():
    im=blank(); fr(im,1,3,6,6); ro(im,1,3,14,13); fr(im,2,7,13,12,BG)
    px(im,[(3,8),(4,8),(5,8),(3,9),(3,10),(3,11)],HL); return im
def i_save():
    im=blank(); ro(im,1,1,14,14); fr(im,2,2,13,13,BG)
    fr(im,3,2,12,5); fr(im,4,8,11,13); fr(im,5,9,10,12,BG); return im
def i_print():
    im=blank(); fr(im,3,2,12,5); fr(im,2,4,13,11); fr(im,3,5,12,10,BG)
    fr(im,4,10,11,14); fr(im,5,11,10,13,BG); px(im,[(11,6),(12,6)],HL); return im
def i_cut():
    im=blank()
    for i in range(8): px(im,[(4+i,12-i)]); px(im,[(11-i,12-i)])
    ro(im,2,11,4,13); ro(im,11,11,13,13); return im
def i_copy():
    im=blank(); ro(im,1,3,10,14); ro(im,5,1,14,12); fr(im,6,2,13,11,BG)
    for y in range(4,10): px(im,[(7,y),(9,y),(11,y)],HL); return im
def i_paste():
    im=blank(); ro(im,2,2,13,14); fr(im,3,3,12,13,BG); fr(im,5,1,10,3); fr(im,6,2,9,2,BG)
    for y in range(5,12,2):
        for x in range(5,11): px(im,[(x,y)],HL)
    return im
def i_undo():
    im=blank()
    for i in range(6): px(im,[(5+i,3)])
    px(im,[(4,4),(11,4),(3,5),(12,5)])
    for y in range(6,11): px(im,[(3,y),(12,y)])
    px(im,[(4,11),(11,11)])
    for x in range(5,11): px(im,[(x,12)])
    px(im,[(2,3),(2,4),(3,3),(2,5),(1,4)],HL); return im
def i_redo(): return mirror(i_undo())
def i_step():
    im=blank()
    for i in range(6):
        for j in range(-(i//2), i//2+1): px(im,[(4+i,7+j)])
    fr(im,12,4,13,11); return im
def i_refresh():
    im=blank(); ro(im,3,3,12,12); fr(im,4,4,11,11,BG); fr(im,4,7,11,8,BG)
    px(im,[(13,6),(14,7),(13,8),(12,7)],FG); px(im,[(2,6),(1,7),(2,8)],FG); return im
def i_page():
    im=blank(); ro(im,1,2,14,13); fr(im,7,2,7,13); fr(im,2,6,6,6); fr(im,8,6,13,6); return im
def i_horiz():
    im=blank()
    for y in (4,8,12):
        for x in range(2,14): px(im,[(x,y)],HL)
    for y in (5,6,7,9,10,11):
        for x in (4,9): px(im,[(x,y)],FG)
    return im
def i_vert():
    im=blank()
    for x in (4,8,12):
        for y in range(2,14): px(im,[(x,y)],HL)
    for x in (5,6,7,9,10,11):
        for y in (4,9): px(im,[(x,y)],FG)
    return im
def i_ghoriz():
    im=blank(); ro(im,1,3,6,7); ro(im,9,3,14,7); ro(im,1,10,6,14); ro(im,9,10,14,14)
    fr(im,2,4,5,6,BG); fr(im,10,4,13,6,BG); fr(im,2,11,5,13,BG); fr(im,10,11,13,13,BG); return im
def i_gvert(): return mirror(i_ghoriz())
def i_title():
    im=blank()
    for x in range(3,13): px(im,[(x,4)])
    for y in range(4,13): px(im,[(7,y),(8,y)])
    return im
def i_bin():
    im=blank()
    fr(im,3,5,4,6); fr(im,7,5,8,6); fr(im,7,9,8,10); fr(im,11,9,12,10)
    px(im,[(5,5),(5,6),(9,9),(9,10),(13,9),(13,10)],FG)
    return im
def i_hex():
    im=blank()
    for x in (3,4,5,6):
        for y in range(5,11): px(im,[(x,y)])
    px(im,[(3,5),(4,5),(5,5),(6,5)],BG); px(im,[(3,5),(4,5)],BG)
    px(im,[(3,8),(4,8),(5,8),(6,8)],BG)
    ro(im,9,5,12,10); fr(im,10,6,11,9,BG); return im
def i_text():
    im=blank()
    for x in range(2,14): px(im,[(x,3)])
    for x in (3,7,11):
        for y in range(5,13): px(im,[(x,y)])
    return im
def i_bmp():
    im=blank(); ro(im,1,3,14,12); fr(im,2,4,13,11,BG)
    for i in range(6): px(im,[(4+i,10-i)],FG)
    for i in range(4): px(im,[(8+i,10-i)],FG)
    return im
def i_filter():
    im=blank()
    for i in range(13): px(im,[(1+i,3)])
    px(im,[(2,4),(13,4)])
    for i in range(5): px(im,[(3+i,5+i)]); px(im,[(12-i,5+i)])
    for y in range(8,14): px(im,[(7,y),(8,y)])
    return im
def i_not():
    im=blank()
    for x in range(3,13): px(im,[(x,7)])
    px(im,[(3,6),(3,5),(4,4),(5,3),(6,3),(10,3),(11,4),(12,5),(12,6),(12,7)],FG)
    return im
def i_rev():
    im=blank()
    for x in range(4,12): px(im,[(x,5)])
    px(im,[(3,5),(2,6),(3,7)],HL); px(im,[(4,6),(4,7)],HL)
    for x in range(4,12): px(im,[(x,10)])
    px(im,[(12,10),(13,9),(12,8)],HL); px(im,[(11,9),(11,8)],HL)
    return im
def i_param():
    im=blank(); ro(im,4,4,11,11); fr(im,5,5,10,10,BG); ro(im,6,6,9,9); fr(im,7,7,8,8,BG)
    for x in range(5,11): px(im,[(x,2),(x,13)])
    for x in (2,13):
        for y in range(5,11): px(im,[(x,y)])
    return im
def i_ruler():
    im=blank(); fr(im,1,6,14,6)
    for x in (2,5,8,11,13):
        for y in range(2,6): px(im,[(x,y)],FG)
        px(im,[(x,7)],FG)
    return im
def i_xtab():
    im=blank()
    for y in range(2,14,2): px(im,[(5,y)])
    for y in range(2,14,2): px(im,[(10,y)])
    for y in (5,10):
        for x in range(2,14): px(im,[(x,y)],HL)
    return im
def i_ytab(): return mirror(i_xtab())
def i_tablock():
    im=blank()
    for x in range(5,11): px(im,[(x,4)])
    px(im,[(5,3),(10,3),(4,5),(11,5)],FG)
    ro(im,3,6,12,12); fr(im,4,7,11,11,BG); px(im,[(7,8),(8,8),(7,9),(8,9)],FG); return im
def i_lockscroll():
    im=blank()
    for x in range(2,7): px(im,[(x,7)])
    px(im,[(1,7),(0,6),(0,8)],HL)
    for x in range(9,14): px(im,[(x,7)])
    px(im,[(14,7),(15,6),(15,8)],HL)
    for x in range(6,10): px(im,[(x,4),(x,10)])
    return im
def i_setstart():
    im=blank()
    for y in range(3,10): px(im,[(8,y)])
    px(im,[(5,9),(6,10),(7,10),(8,10),(9,10),(10,10),(11,9)],HL)
    fr(im,3,12,13,13); return im
def i_contsel():
    im=blank()
    for x in range(2,9): px(im,[(x,7)],HL)
    fr(im,9,4,13,11); fr(im,10,5,12,10,BG); return im
def i_rectsel():
    im=blank(); ro(im,2,3,13,12); fr(im,5,6,10,9,FG); fr(im,6,7,9,8,BG); return im
def i_rowsel():
    im=blank()
    for y in (4,7,10):
        for x in range(2,14): px(im,[(x,y)],FG if y==7 else HL)
    return im
def i_colsel(): return mirror(i_rowsel())
def i_zoomout():
    im=blank(); ro(im,2,2,9,9); fr(im,3,3,8,8,BG)
    for i in range(5): px(im,[(4+i,12-i)]); px(im,[(5+i,12-i)])
    for x in range(4,8): px(im,[(x,5),(x,6)],HL); return im
def i_zoomin():
    im=blank(); ro(im,2,2,9,9); fr(im,3,3,8,8,BG)
    for i in range(5): px(im,[(4+i,12-i)]); px(im,[(5+i,12-i)])
    for x in range(4,8): px(im,[(x,5),(x,6)],HL)
    for y in range(4,8): px(im,[(5,y),(6,y)],HL); return im
def i_reset():
    im=blank(); fr(im,3,3,4,12); fr(im,6,7,9,8); fr(im,11,3,12,12); return im
def i_selpar():
    im=blank(); ro(im,1,2,6,7); fr(im,2,3,5,6,BG)
    px(im,[(9,2),(9,3),(9,4),(10,3),(11,2),(11,3),(11,4),(12,3)],FG)
    ro(im,1,9,6,14); fr(im,2,10,5,13,BG)
    px(im,[(9,9),(9,10),(9,11),(10,10),(11,9),(11,10),(11,11),(12,10)],FG)
    return im
def i_output():
    im=blank()
    for x in range(2,8): px(im,[(x,7)])
    px(im,[(8,6),(9,7),(8,8)],HL)
    ro(im,10,4,14,11); return im
def i_saveas():
    im=i_save()
    px(im,[(6,9),(7,9),(8,9),(9,9),(10,9)],HL)
    px(im,[(6,11),(7,11),(8,11),(9,11),(10,11)],HL)
    return im
def i_searchfl():
    im=blank(); ro(im,2,2,9,9); fr(im,3,3,8,8,BG)
    for i in range(5): px(im,[(4+i,12-i)]); px(im,[(5+i,12-i)])
    px(im,[(4,5),(5,5),(6,5),(4,6),(6,6)],HL)
    px(im,[(5,6),(6,7),(7,8),(8,9),(9,10)],BG)
    return im
def i_searchstr():
    im=blank()
    for x in (4,8,12):
        for y in range(2,14): px(im,[(x,y)])
    for y in (4,8,12):
        for x in range(2,14): px(im,[(x,y)])
    return im
def i_quitsearch():
    im=i_searchstr()
    for i in range(10):
        px(im,[(3+i,3+i)]); px(im,[(12-i,3+i)])
    return im
def i_bmark():
    im=blank()
    ro(im,2,2,12,4); fr(im,3,3,11,3,BG)
    px(im,[(4,5),(5,6),(6,7),(7,8),(8,9),(9,10),(10,11),(11,12)],FG)
    px(im,[(6,5),(7,6),(8,7),(9,8),(10,9),(11,10),(12,11),(13,12)],FG)
    return im
def i_bmarkn():
    im=i_bmark(); px(im,[(14,4),(15,4),(14,5),(15,5)],HL); return im
def i_bmarkp():
    im=i_bmark(); px(im,[(14,9),(15,9),(14,10),(15,10)],HL); return im
def i_bmarkc():
    im=i_bmark()
    for i in range(5):
        px(im,[(9+i,9+i)]); px(im,[(13-i,9+i)])
    return im
def i_goto():
    im=blank(); ro(im,4,4,11,11); fr(im,7,7,8,8,BG)
    px(im,[(7,1),(8,1),(7,2),(8,2),(7,13),(8,13),(7,14),(8,14)],FG)
    px(im,[(1,7),(2,7),(1,8),(2,8),(13,7),(14,7),(13,8),(14,8)],FG)
    return im

FUNCS = [i_new,i_open,i_save,i_print,i_cut,i_copy,i_paste,i_undo,i_redo,i_step,i_refresh,
         i_page,i_horiz,i_vert,i_ghoriz,i_gvert,i_title,
         i_bin,i_hex,i_text,i_bmp,i_filter,i_not,i_rev,i_param,i_ruler,i_xtab,i_ytab,i_tablock,
         i_lockscroll,i_setstart,i_contsel,i_rectsel,i_rowsel,i_colsel,
         i_zoomout,i_zoomin,i_reset,i_selpar,i_output,i_saveas,i_searchfl,i_searchstr,i_quitsearch,
         i_bmark,i_bmarkn,i_bmarkp,i_bmarkc,i_goto]
NAMES = ["新建","打开","保存","打印","剪切","复制","粘贴","撤销","重做","单步","刷新",
         "分页","横排","竖排","组横","组竖","标题",
         "二进制","十六进制","文本","位图","过滤","取反","倒序","参数","标尺","XTAB","YTAB","TAB锁",
         "联动","设起点","连续","块选","行选","列选",
         "缩小","放大","恢复","选参","输出","另存","帧查","码查","退查",
         "书签","下签","上签","清签","定位"]

N = len(FUNCS)
sw = W * N
strip = [[BG]*sw for _ in range(H)]
for idx, f in enumerate(FUNCS):
    ic = f()
    for y in range(H):
        for x in range(W):
            strip[y][idx*W + x] = ic[y][x]

raw = bytearray()
for y in range(H):
    raw += b'\x00'
    for x in range(sw):
        r, g, b, a = strip[y][x]
        raw += bytes((r, g, b, a))
png = (b'\x89PNG\r\n\x1a\n'
       + chunk(b'IHDR', struct.pack('>IIBBBBB', sw, H, 8, 6, 0, 0, 0))
       + chunk(b'IDAT', zlib.compress(bytes(raw)))
       + chunk(b'IEND', b''))
open('src/res/toolbar.png', 'wb').write(png)
print(f"toolbar.png: {sw}x{H} ({N} icons)")
