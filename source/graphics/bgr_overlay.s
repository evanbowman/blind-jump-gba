
@{{BLOCK(bgr_overlay)

@=======================================================================
@
@	bgr_overlay, 1248x8@4, 
@	Transparent color : FF,00,FF
@	+ palette 256 entries, not compressed
@	+ 156 tiles not compressed
@	Total size: 512 + 4992 = 5504
@
@	Time-stamp: 2020-03-18, 20:52:09
@	Exported by Cearn's GBA Image Transmogrifier, v
@	( http://www.coranac.com/projects/#grit )
@
@=======================================================================

	.section .rodata
	.align	2
	.global bgr_overlayTiles		@ 4992 bytes
bgr_overlayTiles:
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00033000,0x00300300,0x00300300,0x00300300,0x00033000,0x00000000
	.word 0x00000000,0x00000000,0x00033000,0x00030000,0x00030000,0x00030000,0x00030000,0x00000000
	.word 0x00000000,0x00000000,0x00033300,0x00300000,0x00333300,0x00000300,0x00333300,0x00000000
	.word 0x00000000,0x00000000,0x00033300,0x00300000,0x00033000,0x00300000,0x00033300,0x00000000
	.word 0x00000000,0x00000000,0x00300300,0x00300300,0x00333000,0x00300000,0x00300000,0x00000000
	.word 0x00000000,0x00000000,0x00333300,0x00000300,0x00333300,0x00300000,0x00033300,0x00000000
	.word 0x00000000,0x00000000,0x00033000,0x00000300,0x00033300,0x00300300,0x00033000,0x00000000

	.word 0x00000000,0x00000000,0x00333300,0x00300000,0x00030000,0x00003000,0x00003000,0x00000000
	.word 0x00000000,0x00000000,0x00033000,0x00300300,0x00033000,0x00300300,0x00033000,0x00000000
	.word 0x00000000,0x00000000,0x00033000,0x00300300,0x00333000,0x00300000,0x00033000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00333000,0x00300300,0x00330300,0x33303033,0x00000000
	.word 0x00003000,0x00000300,0x00000300,0x00330300,0x03003300,0x03003300,0x30330033,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00033000,0x00000300,0x03000300,0x30333033,0x00000000
	.word 0x00030000,0x00300000,0x00300000,0x00303300,0x00330030,0x00330030,0x33003303,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00003300,0x00003030,0x00300330,0x33033303,0x00000000

	.word 0x00330000,0x00003000,0x00003000,0x00333300,0x00003000,0x00003000,0x00003003,0x00000330
	.word 0x00000000,0x00000000,0x00000000,0x00033000,0x00300300,0x00333000,0x33030003,0x00003330
	.word 0x00030000,0x00003000,0x00003000,0x00333000,0x03003000,0x03003300,0x33003033,0x00000000
	.word 0x00300000,0x00030000,0x00000000,0x00003000,0x00003000,0x00003300,0x33330033,0x00000000
	.word 0x03000000,0x00300000,0x00000000,0x00300000,0x00300000,0x00300003,0x00300030,0x00033300
	.word 0x00030000,0x00003000,0x03003000,0x03003000,0x00333000,0x03003300,0x33003033,0x00000000
	.word 0x00030000,0x00003000,0x00003000,0x00003000,0x00003000,0x00033300,0x33303033,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00033033,0x03330330,0x03030030,0x33030003,0x00000000

	.word 0x00000000,0x00000000,0x00000000,0x00330330,0x00303300,0x00300300,0x33000033,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00033000,0x00300300,0x00300300,0x33033033,0x00000000
	.word 0x00000000,0x00000000,0x00003000,0x00333000,0x03003000,0x03303000,0x30033003,0x00000330
	.word 0x00000000,0x00000000,0x00000000,0x00033300,0x00300300,0x00333300,0x30300033,0x03300000
	.word 0x00000000,0x00000000,0x00000000,0x00333000,0x00003000,0x00003000,0x33330333,0x00000000
	.word 0x00000000,0x00000000,0x00033000,0x00003000,0x00330000,0x00300300,0x33033333,0x00000000
	.word 0x00030000,0x00030000,0x03333300,0x00030000,0x00030000,0x00333000,0x33030333,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00300300,0x00300300,0x00300300,0x33333033,0x00000000

	.word 0x00000000,0x00000000,0x00000000,0x30003000,0x03003300,0x00303030,0x33030003,0x00000000
	.word 0x00000000,0x00000000,0x00000300,0x30030030,0x03003030,0x00330330,0x33300303,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00300300,0x00033000,0x00303000,0x33000333,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x03003000,0x03003000,0x03030000,0x30300003,0x00033330
	.word 0x00000000,0x00000000,0x00000000,0x00333300,0x00030000,0x03003000,0x30333333,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00003000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00030000,0x00030000,0x00003000
	.word 0x22222222,0x22222222,0x22222222,0x22222222,0x22222222,0x22222222,0x22222222,0x22222222

	.word 0x22222222,0x22222222,0x22211222,0x22122122,0x22122122,0x22122122,0x22211222,0x22222222
	.word 0x22222222,0x22222222,0x22211222,0x22212222,0x22212222,0x22212222,0x22212222,0x22222222
	.word 0x22222222,0x22222222,0x22211122,0x22122222,0x22111122,0x22222122,0x22111122,0x22222222
	.word 0x22222222,0x22222222,0x22211122,0x22122222,0x22211222,0x22122222,0x22211122,0x22222222
	.word 0x22222222,0x22222222,0x22122122,0x22122122,0x22111222,0x22122222,0x22122222,0x22222222
	.word 0x22222222,0x22222222,0x22111122,0x22222122,0x22111122,0x22122222,0x22211122,0x22222222
	.word 0x22222222,0x22222222,0x22211222,0x22222122,0x22211122,0x22122122,0x22211222,0x22222222
	.word 0x22222222,0x22222222,0x22111122,0x22122222,0x22212222,0x22221222,0x22221222,0x22222222

	.word 0x22222222,0x22222222,0x22211222,0x22122122,0x22211222,0x22122122,0x22211222,0x22222222
	.word 0x22222222,0x22222222,0x22211222,0x22122122,0x22111222,0x22122222,0x22211222,0x22222222
	.word 0x22222222,0x22222222,0x22222222,0x22111222,0x22122122,0x22112122,0x22121222,0x22222222
	.word 0x22221222,0x22222122,0x22222122,0x22112122,0x21221122,0x21221122,0x22112222,0x22222222
	.word 0x22222222,0x22222222,0x22222222,0x22211222,0x22222122,0x21222122,0x22111222,0x22222222
	.word 0x22212222,0x22122222,0x22122222,0x22121122,0x22112212,0x22112212,0x22221122,0x22222222
	.word 0x22222222,0x22222222,0x22222222,0x22221122,0x22221212,0x22122112,0x22211122,0x22222222
	.word 0x22112222,0x22221222,0x22221222,0x22111122,0x22221222,0x22221222,0x22221222,0x22222112

	.word 0x22222222,0x22222222,0x22211222,0x22122122,0x22122122,0x22111222,0x22212221,0x22221112
	.word 0x22212222,0x22221222,0x22221222,0x22111222,0x21221222,0x21221222,0x21221222,0x22222222
	.word 0x22122222,0x22212222,0x22222222,0x22221222,0x22221222,0x22221222,0x22221222,0x22222222
	.word 0x21222222,0x22122222,0x22222222,0x22122222,0x22122222,0x22122221,0x22122212,0x22211122
	.word 0x22212222,0x22221222,0x21221222,0x21221222,0x22111222,0x21221222,0x21221222,0x22222222
	.word 0x22212222,0x22221222,0x22221222,0x22221222,0x22221222,0x22221222,0x22221222,0x22222222
	.word 0x22222222,0x22222222,0x22222222,0x22211211,0x21112112,0x21212212,0x21212211,0x22222222
	.word 0x22222222,0x22222222,0x22222222,0x22112112,0x22121122,0x22122122,0x22122112,0x22222222

	.word 0x22222222,0x22222222,0x22222222,0x21211222,0x22122122,0x22122122,0x22211222,0x22222222
	.word 0x22222222,0x22222222,0x22111222,0x21221222,0x21221222,0x22111222,0x22221222,0x22222122
	.word 0x22222222,0x22222222,0x22111222,0x22122122,0x22122122,0x22111222,0x22122222,0x21122222
	.word 0x22222222,0x22222222,0x22222222,0x22111222,0x22221222,0x22221222,0x22221222,0x22222222
	.word 0x22222222,0x22222222,0x22211222,0x22221222,0x22112222,0x22122122,0x22211222,0x22222222
	.word 0x22212222,0x22212222,0x21111122,0x22212222,0x22212222,0x22212222,0x22212222,0x22222222
	.word 0x22222222,0x22222222,0x22222222,0x22122122,0x22122122,0x22122122,0x22111222,0x22222222
	.word 0x22222222,0x22222222,0x22222222,0x21222122,0x22122122,0x22212122,0x22221222,0x22222222

	.word 0x22222222,0x22222222,0x22222122,0x12212212,0x21221212,0x22112112,0x22122122,0x22222222
	.word 0x22222222,0x22222222,0x22222222,0x22122122,0x22211222,0x22121222,0x21222122,0x22222222
	.word 0x22222222,0x22222222,0x22222222,0x21221222,0x21221222,0x21212222,0x22122212,0x22211122
	.word 0x22222222,0x22222222,0x22222222,0x22111122,0x22212222,0x22221222,0x22111122,0x22222222
	.word 0x22222222,0x22222222,0x22222222,0x22222222,0x22222222,0x22222222,0x22221222,0x22222222
	.word 0x22222222,0x22222222,0x22222222,0x22222222,0x22222222,0x22212222,0x22212222,0x22221222
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000

	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000

	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000

	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000

	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000

	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000

	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000

	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000

	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000

	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000

	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.word 0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
	.size	bgr_overlayTiles, .-bgr_overlayTiles

	.section .rodata
	.align	2
	.global bgr_overlayPal		@ 512 bytes
bgr_overlayPal:
	.hword 0x7C1F,0x296E,0x6FBF,0x04FE,0x0000,0x0000,0x0000,0x0000
	.hword 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
	.hword 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
	.hword 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
	.hword 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
	.hword 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
	.hword 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
	.hword 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000

	.hword 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
	.hword 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
	.hword 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
	.hword 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
	.hword 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
	.hword 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
	.hword 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
	.hword 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000

	.hword 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
	.hword 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
	.hword 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
	.hword 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
	.hword 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
	.hword 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
	.hword 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
	.hword 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000

	.hword 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
	.hword 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
	.hword 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
	.hword 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
	.hword 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
	.hword 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
	.hword 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
	.hword 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
	.size	bgr_overlayPal, .-bgr_overlayPal

@}}BLOCK(bgr_overlay)