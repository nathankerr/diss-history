#define _GNU_SOURCE
#include <stdio.h>
#include <math.h>

#include <git2.h>
#include <cairo.h>
#include <poppler.h>

void handle_git_error(int err) {
	if (err < 0) {
		const git_error *e = giterr_last();
		printf("Error %d/%d: %s\n", err, e->klass, e->message);
		exit(err);
	}
}

void exit_if_cairo_surface_status_not_success(cairo_surface_t* surface, char* file, int line) {
	cairo_status_t status = cairo_surface_status(surface);
	if (status != CAIRO_STATUS_SUCCESS) {
		printf("%s:%d: %s\n", file, line, cairo_status_to_string(status));
		exit(1);
	}
}

void exit_if_cairo_status_not_success(cairo_t* cr, char* file, int line) {
	cairo_status_t status = cairo_status(cr);
	if (status != CAIRO_STATUS_SUCCESS) {
		printf("%s:%d: %s\n", file, line, cairo_status_to_string(status));
		exit(1);
	}
}

// if PopplerDocument* is NULL, return error to user
PopplerDocument* open_document(char* filename, GError **error) {
	// resolve path names
	gchar *absolute;
	if (g_path_is_absolute(filename)) {
		absolute = g_strdup(filename);
	} else {
		gchar* dir = g_get_current_dir();
		absolute = g_build_filename(dir, filename, (gchar*)0);
		free(dir);
	}

	gchar *uri = g_filename_to_uri(absolute, NULL, error);
	free(absolute);
	if (uri == NULL) {
		return NULL;
	}

	PopplerDocument* document = poppler_document_new_from_file(uri, NULL, error);
	free(uri);
	return document;
}

void pdf2pngstamp(char *pdf_filename, char *png_filename, char *stamp, int max_page_count) {
	const double PAPER_WIDTH = 1920;
	const double PAPER_HEIGHT = 1080;

	// load the pdf
	GError *error = NULL;
	PopplerDocument *document = open_document(pdf_filename, &error);
	if (document == NULL) {
		printf("%s:%d: %s\n", __FILE__, __LINE__, error->message);
		exit(1);
	}

	// the input pdfs should only have one page
	PopplerPage *page = poppler_document_get_page(document, 0);
	if (page == NULL) {
		printf("%s:%d: %s\n", __FILE__, __LINE__, error->message);
		exit(1);		
	}
	int num_pages = poppler_document_get_n_pages(document);

	// create the image surface
	cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, PAPER_WIDTH, PAPER_HEIGHT);
	exit_if_cairo_surface_status_not_success(surface, __FILE__, __LINE__);
	cairo_t *cr = cairo_create(surface);
	exit_if_cairo_status_not_success(cr, __FILE__, __LINE__);

	double page_width, page_height;
	poppler_page_get_size(page, &page_width, &page_height);
	g_object_unref(page);

	// assuming that all the pages are the same size
	// the sum of the page areas must fit in the paper area
	// paper_area >= scale_factor² * num_pages * page_area
	double paper_area = PAPER_WIDTH * PAPER_HEIGHT;
	double page_area = page_width * page_height;
	double scale_factor = sqrt(paper_area / max_page_count / page_area);
	double scaled_page_width = scale_factor * page_width;
	int nx = (int) round(PAPER_WIDTH / scaled_page_width);
	int ny = (int) round(max_page_count / nx);
	while ((nx * ny) < max_page_count) {
		ny++;
	}

	// adjust scale_factor to fit the new page count
	double scale_factor_width = PAPER_WIDTH / nx / page_width;
	double scale_factor_height = PAPER_HEIGHT / ny / page_height;
	if (scale_factor_width > scale_factor_height) {
		scale_factor = scale_factor_height;
	} else {
		scale_factor = scale_factor_width;
	}

	// if the pages won't fill up the paper, center them on the paper
	double top_margin = (PAPER_HEIGHT - (scale_factor * page_height * ny)) / 2.0;
	double left_margin = (PAPER_WIDTH - (scale_factor * page_width * nx)) / 2.0;
	cairo_translate(cr, left_margin, top_margin);

	// layout the pages on the paper
	cairo_save(cr);
	cairo_scale(cr, scale_factor, scale_factor);
	int page_num;
	for (page_num = 0; page_num < num_pages; page_num++) {
		page = poppler_document_get_page(document, page_num);
		if (page == NULL) {
			printf("%s:%d: %s\n", __FILE__, __LINE__, error->message);
			exit(1);		
		}

		// clip to the page size and render
		cairo_save(cr);

		cairo_rectangle(cr, 0, 0, page_width, page_height);
		cairo_clip(cr);

		poppler_page_render(page, cr);

		cairo_restore(cr);

		// draw a box around the page
		cairo_rectangle(cr, 0, 0, page_width, page_height);
		cairo_stroke(cr);

		// move to where the next page goes
		if ((page_num+1)%nx == 0) {
			cairo_translate(cr, -page_width*(nx-1), page_height);
		} else {
			cairo_translate(cr, page_width, 0);
		}
		
		// FIXME: at times this free would seg. fault.
		// commenting it out causes memory to leak
		// g_object_unref(page);
	}
	cairo_restore(cr);

	// white background
	cairo_save(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_DEST_OVER);
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_paint(cr);
	cairo_restore(cr);

	// stamp
	cairo_set_font_size(cr, 80);
	cairo_text_extents_t text_extent;
	cairo_text_extents(cr, stamp, &text_extent);

	double versionx = (PAPER_WIDTH-text_extent.width)/2.0;
	double versiony = PAPER_HEIGHT - text_extent.height;
	cairo_move_to(cr, versionx, versiony);
	cairo_show_text(cr, stamp);
	cairo_stroke(cr);

	// save the png
	cairo_status_t status = cairo_surface_write_to_png(surface, png_filename);
	if (status != CAIRO_STATUS_SUCCESS) {
		printf("%s:%d: %s\n", __FILE__, __LINE__, cairo_status_to_string(status));
		exit(1);
	}

	// cleanup and finish
	g_object_unref(document);

	exit_if_cairo_status_not_success(cr, __FILE__, __LINE__);
	cairo_destroy(cr);	

	cairo_surface_destroy(surface);
	exit_if_cairo_surface_status_not_success(surface, __FILE__, __LINE__);
}

int main(int argc, char** argv) {
	const char* repo_dirname = "dissertation";

	// open repo
	git_repository *repo;
	int err = git_repository_open_ext(&repo, repo_dirname, 0, NULL);
	handle_git_error(err);

	// setup the repo walker
	git_revwalk *walk;
	err = git_revwalk_new(&walk, repo);
	handle_git_error(err);
	git_revwalk_sorting(walk, GIT_SORT_TOPOLOGICAL|GIT_SORT_REVERSE);

	// find the max page count
	err = git_revwalk_push_range(walk, "7af0f9..HEAD");
	handle_git_error(err);
	int max_page_count = 0;
	git_oid oid;
	while (git_revwalk_next(&oid, walk) != GIT_ITEROVER) {
		char oid_str[41];
		git_oid_fmt(oid_str, &oid);
		oid_str[40] = '\0';

		char* pdf_filename;
		asprintf(&pdf_filename, "%s.pdf", oid_str);

		GError *error = NULL;
		PopplerDocument *document = open_document(pdf_filename, &error);
		if (document == NULL) {
			printf("%s:%d: %s\n", __FILE__, __LINE__, error->message);
			exit(1);
		}

		int num_pages = poppler_document_get_n_pages(document);
		if (num_pages > max_page_count) {
			max_page_count = num_pages;
		}

		g_object_unref(document);
		free(pdf_filename);
	}
	git_revwalk_free(walk);
	printf("max_page_count: %d\n", max_page_count);

	// make a png for each commit
	// git_revwalk_reset(walk);
	err = git_revwalk_new(&walk, repo);
	handle_git_error(err);
	git_revwalk_sorting(walk, GIT_SORT_TOPOLOGICAL|GIT_SORT_REVERSE);
	err = git_revwalk_push_range(walk, "7af0f9..HEAD");
	handle_git_error(err);

	int n = 0;
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;
	while (git_revwalk_next(&oid, walk) != GIT_ITEROVER) {
		char oid_str[41];
		git_oid_fmt(oid_str, &oid);
		oid_str[40] = '\0';

		// grab the actual commit	
		git_commit *commit;
		err = git_commit_lookup(&commit, repo, &oid);
		handle_git_error(err);

		// figure out the time of the commit
		tzset();
		git_time_t git_time = git_commit_time(commit);
		int offset = git_commit_time_offset(commit);
		git_time += offset*60;

		struct tm *tm_time = gmtime((time_t*) &git_time);
		char time_str[70];
		strftime(time_str, 70, "%F", tm_time);

		char* pdf_filename;
		asprintf(&pdf_filename, "%s.pdf", oid_str);

		char* png_filename;
		asprintf(&png_filename, "%03d.png", n);

		// short version of the hash
		char hash[8];
		git_oid_tostr(hash, 8, &oid);

		char* stamp;
		asprintf(&stamp, "%s %s", hash, time_str);

		printf("%s %s %s\n", png_filename, hash, time_str);
		fflush(NULL);

		pdf2pngstamp(pdf_filename, png_filename, stamp, max_page_count);

		free(pdf_filename);
		free(png_filename);
		free(stamp);

		n++;
	}

	// cleanup
	git_revwalk_free(walk);
	git_repository_free(repo);

	printf("done\n");

	return 0;
}