#define _GNU_SOURCE
#include <stdio.h>
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

void pdf2pngstamp(char *pdf_filename, char *png_filename, char *stamp) {
	const double width = 1920;
	const double height = 1080;

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

	// create the image surface
	cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	exit_if_cairo_surface_status_not_success(surface, __FILE__, __LINE__);
	cairo_t *cr = cairo_create(surface);
	exit_if_cairo_status_not_success(cr, __FILE__, __LINE__);

	// render
	poppler_page_render(page, cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_DEST_OVER);
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_paint(cr);

	// stamp
	cairo_text_extents_t text_extent;
	cairo_text_extents(cr, stamp, &text_extent);

	double versionx = (width-text_extent.width)/2.0;
	double versiony = height - text_extent.height;
	cairo_move_to(cr, versionx, versiony);
	cairo_show_text(cr, stamp);
	cairo_set_line_width(cr,1);
	cairo_rectangle(cr, versionx-5, versiony+5, text_extent.width+11, -text_extent.height-9);
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

	err = git_revwalk_push_range(walk, "7af0f9..HEAD");
	handle_git_error(err);

	// walk
	int n = 0;
	git_oid oid;
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

		pdf2pngstamp(pdf_filename, png_filename, stamp);

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