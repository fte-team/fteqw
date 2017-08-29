CC=i686-pc-mingw32-gcc

all:

NAMES=	fixedemu	altwater	bloom_blur	bloom_filter	bloom_final	colourtint	crepuscular_opaque	crepuscular_rays	crepuscular_sky	depthonly	default2d	defaultadditivesprite		defaultskin	defaultsky	defaultskybox	defaultfill	defaultsprite	defaultwall	defaultwarp	defaultgammacb	drawflat_wall	lpp_depthnorm	lpp_light	lpp_wall	postproc_fisheye	postproc_panorama	postproc_laea	postproc_stereographic	postproc_equirectangular	fxaa	underwaterwarp	menutint	terrain	rtlight


ALLNAMES+=$(foreach v,$(NAMES),glsl/$v.glsl)
ALLNAMES+=$(foreach v,$(NAMES),hlsl9/$v.hlsl)
ALLNAMES+=$(foreach v,$(NAMES),hlsl11/$v.hlsl)
ALLNAMES:=$(realpath $(ALLNAMES))

VKNAMES=$(realpath $(foreach v,$(NAMES),vulkan/$v.glsl))
VKNAMES+=postproc_fisheye
VKNAMES:=$(foreach v,$(VKNAMES),vulkanblobs/$(notdir $(basename $v)).fvb)
ALLNAMES+=$(VKNAMES)

generatebuiltinsl: generatebuiltinsl.c
	$(CC) $< -o $@

makevulkanblob: makevulkanblob.c
	$(CC) $< -o $@

vulkanblobs/%.fvb: vulkan/%.glsl makevulkanblob vulkan/sys/defs.h vulkan/sys/fog.h vulkan/sys/offsetmapping.h vulkan/sys/skeletal.h
	./makevulkanblob $< $@

#vulkanblobs/%.fvb: glsl/%.glsl makevulkanblob vulkan/sys/defs.h vulkan/sys/fog.h vulkan/sys/offsetmapping.h vulkan/sys/skeletal.h
#	./makevulkanblob $< $@


all: generatebuiltinsl $(ALLNAMES)
	which glslangValidator
	./generatebuiltinsl