# The core sources, and the C generated from assets into build/gen, for every Makefile that
# builds the core (set ROOT first if the repo root isn't ../..). platform/pc/CMakeLists.txt
# does the same itself.
CORE_MK_GOAL := $(.DEFAULT_GOAL)
ROOT     ?= ../..
CORE     := $(ROOT)/core
GEN      := $(ROOT)/build/gen
PYTHON   ?= python3
CORE_SRC := world.c place.c
GEN_SRC  := guy_mesh.c default_place.c
CORE_OBJ := $(CORE_SRC:.c=.o) $(GEN_SRC:.c=.o)

# A failed generator must not leave a half-written file that looks up to date.
.DELETE_ON_ERROR:

$(GEN)/guy_mesh.c: $(ROOT)/tools/obj2c.py $(ROOT)/tools/objload.py $(ROOT)/tools/texload.py $(ROOT)/assets/model/guy.obj $(ROOT)/assets/texture/guy_base.png $(ROOT)/assets/texture/guy_coat.png
	@mkdir -p $(dir $@)
	$(PYTHON) $< $(ROOT)/assets/model/guy.obj $@ guy_mesh 255,255,255 $(ROOT)/assets/texture/guy_base.png guy_texture $(ROOT)/assets/texture/guy_coat.png

$(GEN)/default_place.c: $(ROOT)/tools/placec.py $(ROOT)/tools/objload.py $(ROOT)/tools/texload.py $(ROOT)/assets/scene/course.json $(wildcard $(ROOT)/assets/model/* $(ROOT)/assets/texture/*)
	@mkdir -p $(dir $@)
	$(PYTHON) $< $(ROOT)/assets/scene/course.json $@ default_place

# Keep these rules from becoming the default target.
.DEFAULT_GOAL := $(CORE_MK_GOAL)
