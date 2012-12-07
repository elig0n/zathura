/* See LICENSE file for license and copyright information */

#include "print.h"
#include "document.h"
#include "render.h"
#include "page.h"

#include <girara/utils.h>
#include <girara/statusbar.h>

static void cb_print_draw_page(GtkPrintOperation* print_operation,
                               GtkPrintContext* context, gint page_number, zathura_t* zathura);
static void cb_print_end(GtkPrintOperation* print_operation, GtkPrintContext*
                         context, zathura_t* zathura);
static void cb_print_request_page_setup(GtkPrintOperation* print_operation,
                                        GtkPrintContext* context, gint page_number, GtkPageSetup* setup, zathura_t*
                                        zathura);

void
print(zathura_t* zathura)
{
  g_return_if_fail(zathura           != NULL);
  g_return_if_fail(zathura->document != NULL);

  GtkPrintOperation* print_operation = gtk_print_operation_new();

  /* print operation settings */
  if (zathura->print.settings != NULL) {
    gtk_print_operation_set_print_settings(print_operation, zathura->print.settings);
  }

  if (zathura->print.page_setup != NULL) {
    gtk_print_operation_set_default_page_setup(print_operation, zathura->print.page_setup);
  }

  gtk_print_operation_set_allow_async(print_operation, TRUE);
  gtk_print_operation_set_n_pages(print_operation, zathura_document_get_number_of_pages(zathura->document));
  gtk_print_operation_set_current_page(print_operation, zathura_document_get_current_page_number(zathura->document));
  gtk_print_operation_set_use_full_page(print_operation, TRUE);
  gtk_print_operation_set_embed_page_setup(print_operation, TRUE);

  /* print operation signals */
  g_signal_connect(print_operation, "draw-page",          G_CALLBACK(cb_print_draw_page),          zathura);
  g_signal_connect(print_operation, "end-print",          G_CALLBACK(cb_print_end),                zathura);
  g_signal_connect(print_operation, "request-page-setup", G_CALLBACK(cb_print_request_page_setup), zathura);

  /* print */
  GtkPrintOperationResult result = gtk_print_operation_run(print_operation,
                                   GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG, NULL, NULL);

  if (result == GTK_PRINT_OPERATION_RESULT_APPLY) {
    if (zathura->print.settings != NULL) {
      g_object_unref(zathura->print.settings);
    }
    if (zathura->print.page_setup != NULL) {
      g_object_unref(zathura->print.page_setup);
    }

    /* save previous settings */
    zathura->print.settings   = g_object_ref(gtk_print_operation_get_print_settings(print_operation));
    zathura->print.page_setup = g_object_ref(gtk_print_operation_get_default_page_setup(print_operation));
  } else if (result == GTK_PRINT_OPERATION_RESULT_ERROR) {
    girara_error("Error occured while printing progress");
  }

  g_object_unref(print_operation);
}

static void
cb_print_end(GtkPrintOperation* UNUSED(print_operation), GtkPrintContext*
             UNUSED(context), zathura_t* zathura)
{
  if (zathura == NULL || zathura->ui.session == NULL || zathura->document == NULL) {
    return;
  }

  const char* file_path = zathura_document_get_path(zathura->document);

  if (file_path != NULL) {
    girara_statusbar_item_set_text(zathura->ui.session,
                                   zathura->ui.statusbar.file, file_path);
  }
}

static void
cb_print_draw_page(GtkPrintOperation* UNUSED(print_operation), GtkPrintContext*
                   context, gint page_number, zathura_t* zathura)
{
  if (context == NULL || zathura == NULL || zathura->document == NULL ||
      zathura->ui.session == NULL || zathura->ui.statusbar.file == NULL) {
    return;
  }

  /* update statusbar */
  char* tmp = g_strdup_printf("Printing %d...", page_number);
  girara_statusbar_item_set_text(zathura->ui.session,
                                 zathura->ui.statusbar.file, tmp);
  g_free(tmp);

  /* render page */
  cairo_t* cairo       = gtk_print_context_get_cairo_context(context);
  zathura_page_t* page = zathura_document_get_page(zathura->document, page_number);
  if (cairo == NULL || page == NULL) {
    return;
  }

  /* we need a temporary cairo object since the cairo object from the print
   * context doesn't have a width or height */
  const gdouble width = gtk_print_context_get_width(context);
  const gdouble height = gtk_print_context_get_height(context);

  cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);
  if (surface == NULL) {
    return;
  }

  cairo_t* temp_cairo = cairo_create(surface);
  if (cairo == NULL) {
    cairo_surface_destroy(surface);
    return;
  }

  /* white background */
  cairo_save(cairo);
  cairo_set_source_rgb(cairo, 1, 1, 1);
  cairo_rectangle(cairo, 0, 0, width, height);
  cairo_fill(cairo);
  cairo_restore(cairo);

  /* render the page to the temporary surface */
  girara_debug("printing page %d ...", page_number);
  render_lock(zathura->sync.render_thread);
  zathura_page_render(page, temp_cairo, true);
  render_unlock(zathura->sync.render_thread);

  /* copy temporary surface */
  cairo_set_source_surface(cairo, surface, 0.0, 0.0);
  cairo_paint(cairo);
  cairo_destroy(temp_cairo);
  cairo_surface_destroy(surface);
}

static void
cb_print_request_page_setup(GtkPrintOperation* UNUSED(print_operation),
                            GtkPrintContext* UNUSED(context), gint page_number, GtkPageSetup* setup,
                            zathura_t* zathura)
{
  if (zathura == NULL || zathura->document == NULL) {
    return;
  }

  zathura_page_t* page = zathura_document_get_page(zathura->document, page_number);
  double width  = zathura_page_get_width(page);
  double height = zathura_page_get_height(page);

  if (width > height) {
    gtk_page_setup_set_orientation(setup, GTK_PAGE_ORIENTATION_LANDSCAPE);
  } else {
    gtk_page_setup_set_orientation(setup, GTK_PAGE_ORIENTATION_PORTRAIT);
  }
}
