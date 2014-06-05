CFLAGS=`pkg-config --cflags cairo poppler-glib libgit2` -Wall -Werror -g
LDFLAGS=`pkg-config --libs cairo poppler-glib libgit2` -lm

REPO := dissertation
GIT := cd $(REPO); git

VERSIONS := $(shell $(GIT) log --format=%H 7af0f9..HEAD)
PDFS = $(VERSIONS:=.pdf)

all: mksingle mkhistory $(PDFS)
	./mkhistory
	rm -f history.mp4
	ffmpeg -r 1 -i %03d.png -r 30 history.mp4
	vlc history.mp4

%.pdf:
	echo making version $*
	$(GIT) checkout -f $*
	cd $(REPO); $(MAKE) paper.pdf
	./mksingle $(REPO)/paper.pdf $@
	#mv $(REPO)/paper.pdf $*.pdf
	cd $(REPO); $(MAKE) clean
	$(GIT) checkout -f master

%-single.pdf: %.pdf
	./mksingle $< $@

clean:
	rm -rf *.pdf *.png
	rm -f mksingle mkhistory
	rm -f history.mp4
