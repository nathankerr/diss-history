#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <cairo.h>
#include <cairo-pdf.h>
#include <poppler.h>

// toggle to display boxes for debugging
#define DISPLAY_BOXES

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

int main(int argc, char** argv) {
	if (argc != 3) {
		printf("Usage: %s <input.pdf> <output.pdf>\n", argv[0]);
		exit(1);
	}
	char* input_filename = argv[1];
	char* output_filename = argv[2];

	// 1 pt = 1/27 in
	// 1 in = 2.54 cm
	// A4 210x297mm = 595.224x841.824 
	const double PAPER_WIDTH = 1920;
	const double PAPER_HEIGHT = 1080;

	// create a landscape surface for the paper
	cairo_surface_t* surface = cairo_pdf_surface_create(output_filename, PAPER_WIDTH, PAPER_HEIGHT);
	exit_if_cairo_surface_status_not_success(surface, __FILE__, __LINE__);

	cairo_t *cr = cairo_create(surface);
	exit_if_cairo_status_not_success(cr, __FILE__, __LINE__);

	// load the pdf
	GError *error = NULL;
	PopplerDocument *document = open_document(input_filename, &error);
	if (document == NULL) {
		printf("%s:%d: %s\n", __FILE__, __LINE__, error->message);
		exit(1);
	}

	int num_pages = poppler_document_get_n_pages(document);

	PopplerPage *page = poppler_document_get_page(document, 0);
	if (page == NULL) {
		printf("%s:%d: %s\n", __FILE__, __LINE__, error->message);
		exit(1);		
	}

	double page_width, page_height;
	poppler_page_get_size(page, &page_width, &page_height);
	g_object_unref(page);

	// assuming that all the pages are the same size
	// the sum of the page areas must fit in the paper area
	// paper_area >= scale_factor² * num_pages * page_area
	double paper_area = PAPER_WIDTH * PAPER_HEIGHT;
	double page_area = page_width * page_height;
	double scale_factor = sqrt(paper_area / num_pages / page_area);
	double scaled_page_width = scale_factor * page_width;
	int nx = (int) round(PAPER_WIDTH / scaled_page_width);
	int ny = (int) round(num_pages / nx);
	while ((nx * ny) < num_pages) {
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
		PopplerPage *page = poppler_document_get_page(document, page_num);
		if (page == NULL) {
			printf("%s:%d: %s\n", __FILE__, __LINE__, error->message);
			exit(1);		
		}

		// clip to the page size and render
		cairo_save(cr);

		cairo_rectangle(cr, 0, 0, page_width, page_height);
		cairo_clip(cr);

		poppler_page_render_for_printing(page, cr);

		cairo_restore(cr);

#ifdef DISPLAY_BOXES
		cairo_rectangle(cr, 0, 0, page_width, page_height);
		cairo_stroke(cr);
#endif

		// move to where the next page goes
		if ((page_num+1)%nx == 0) {
			cairo_translate(cr, -page_width*(nx-1), page_height);
		} else {
			cairo_translate(cr, page_width, 0);
		}
		
		g_object_unref(page);
	}
	cairo_restore(cr);

	// cleanup and finish
	g_object_unref(document);

	exit_if_cairo_status_not_success(cr, __FILE__, __LINE__);
	cairo_destroy(cr);	

	cairo_surface_destroy(surface);
	exit_if_cairo_surface_status_not_success(surface, __FILE__, __LINE__);
	return 0;
}