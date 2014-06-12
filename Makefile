CFLAGS=`pkg-config --cflags cairo poppler-glib libgit2` -Wall -Werror -g -std=c99
LDFLAGS=`pkg-config --libs cairo poppler-glib libgit2` -lm

REPO := dissertation
GIT := cd $(REPO); git

REPO := dissertation

UNAME := $(shell uname)
ifeq ($(UNAME), Linux)
	MOVIEPLAYER := vlc
endif
ifeq ($(UNAME), Darwin)
	MOVIEPLAYER := open
endif

VERSIONS := $(shell $(GIT) log --format=%H)
PDFS = $(VERSIONS:=.pdf)

all: mksingle mkhistory $(PDFS)
	#$(GIT) checkout -f master
	#./mkhistory
	rm -f history.mkv
	./ffmpeg -r 3 -i %03d.png -r 30 history.mkv
	$(MOVIEPLAYER) history.avi

%.pdf:
	echo making version $*
	$(GIT) checkout -f $*
	# cd $(REPO); $(MAKE) paper.pdf
	# mv $(REPO)/paper.pdf $*.pdf
	# rmdir $(REPO)/prototype
	# cd $(REPO); ln -s ../prototype
	# cd $(REPO); echo "%\c" > prototype.tex.new && cat prototype.tex >> prototype.tex.new && mv prototype.tex.new prototype.tex
	cd $(REPO); $(MAKE) full.pdf
	mv $(REPO)/full.pdf $*.pdf
	cd $(REPO); $(MAKE) clean
	$(GIT) checkout -f master

# this is the prep for merge, so the diss stuff is no longer in the root dir
.PHONY: e88f24be65434de3747e4f39360432433549d6f5.pdf
e88f24be65434de3747e4f39360432433549d6f5.pdf:

.PHONY: 3619d313aecbd0200a9422aeb4b776e75bc9e722.pdf
3619d313aecbd0200a9422aeb4b776e75bc9e722.pdf:

# lyx
.PHONY: 2fb177d1fcb0f0c85de0ada996c1b7f3493203c6.pdf
2fb177d1fcb0f0c85de0ada996c1b7f3493203c6.pdf:

.PHONY: 3aa5728a9e98fd78bf01593e715b4e74ccf96a6e.pdf
3aa5728a9e98fd78bf01593e715b4e74ccf96a6e.pdf:

.PHONY: ee9a47918ea5fb4d6a27e23f68732e8ad921ea11.pdf
ee9a47918ea5fb4d6a27e23f68732e8ad921ea11.pdf:

.PHONY: 9e040916e59c35a419d9cf5c645160acd903cf11.pdf
9e040916e59c35a419d9cf5c645160acd903cf11.pdf:

# did not want to compile
# .PHONY: 9a12d2d8d0969fe009c3e317296d2f47afc358ab.pdf
# 9a12d2d8d0969fe009c3e317296d2f47afc358ab.pdf:

%-single.pdf: %.pdf
	./mksingle $< $@

clean:
	# rm -rf *.pdf
	rm -rf *.png
	rm -f mksingle mkhistory
	rm -f history.mp4
