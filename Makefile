CFLAGS=`pkg-config --cflags cairo poppler-glib libgit2` -Wall -Werror -g
LDFLAGS=`pkg-config --libs cairo poppler-glib libgit2` -lm

REPO := dissertation
GIT := cd $(REPO); git

UNAME := $(shell uname)
ifeq ($(UNAME), Linux)
	MOVIEPLAYER := vlc
endif
ifeq ($(UNAME), Darwin)
	MOVIEPLAYER := open
endif

VERSIONS := $(shell $(GIT) log --format=%H 7af0f9..HEAD)
PDFS = $(VERSIONS:=.pdf)

all: mksingle mkhistory $(PDFS)
	./mkhistory
	rm -f history.avi
	ffmpeg -r 3 -i %03d.png -r 30 history.avi
	$(MOVIEPLAYER) history.avi

%.pdf:
	echo making version $*
	$(GIT) checkout -f $*
	cd $(REPO); $(MAKE) paper.pdf
	mv $(REPO)/paper.pdf $*.pdf
	cd $(REPO); $(MAKE) clean
	$(GIT) checkout -f master

%-single.pdf: %.pdf
	./mksingle $< $@

clean:
	# rm -rf *.pdf
	rm -rf *.png
	rm -f mksingle mkhistory
	rm -f history.mp4
